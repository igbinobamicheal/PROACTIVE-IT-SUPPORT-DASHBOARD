#define NOMINMAX
#include "Application/AgentDaemon.h"
#include "Infrastructure/Logging/Logger.h"
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <bcrypt.h>

namespace Application {

AgentDaemon::AgentDaemon(std::unique_ptr<Domain::RepositoryInterfaces::IConfigRepository> configRepo,
                         std::unique_ptr<Domain::ServiceInterfaces::IMetricCollector> metricCollector,
                         std::unique_ptr<Domain::ServiceInterfaces::IHttpSender> httpSender,
                         const std::string& cacheFilePath)
    : configRepo_(std::move(configRepo)),
      metricCollector_(std::move(metricCollector)),
      httpSender_(std::move(httpSender)),
      offlineCache_(std::make_unique<Infrastructure::Config::OfflineCache>(cacheFilePath)),
      running_(false),
      connected_(true),
      backoffSeconds_(2),
      consecutiveFailures_(0),
      reconnectTriggered_(false) {}

AgentDaemon::~AgentDaemon() {
    Stop();
}

bool AgentDaemon::Start() {
    using namespace Infrastructure::Logging;
    
    if (running_) {
        return true;
    }

    running_ = true;
    connected_ = true;
    backoffSeconds_ = 2;
    consecutiveFailures_ = 0;
    reconnectTriggered_ = false;

    try {
        metricsThread_ = std::thread(&AgentDaemon::MetricsLoop, this);
        diagnosticsThread_ = std::thread(&AgentDaemon::DiagnosticsLoop, this);
        heartbeatThread_ = std::thread(&AgentDaemon::HeartbeatLoop, this);
        reconnectThread_ = std::thread(&AgentDaemon::ReconnectLoop, this);
        updateThread_ = std::thread(&AgentDaemon::UpdateLoop, this);
        
        Logger::GetInstance().Info("AgentDaemon background worker threads started.");
        return true;
    } catch (const std::exception& e) {
        running_ = false;
        Logger::GetInstance().Error("Failed to start AgentDaemon threads: " + std::string(e.what()));
        return false;
    }
}

void AgentDaemon::Stop() {
    using namespace Infrastructure::Logging;

    if (!running_) {
        return;
    }

    Logger::GetInstance().Info("Stopping AgentDaemon...");
    running_ = false;
    
    // Wake up all sleeping condition variables
    cv_.notify_all();
    cvReconnect_.notify_all();

    // Join all loops
    if (metricsThread_.joinable()) {
        metricsThread_.join();
    }
    if (diagnosticsThread_.joinable()) {
        diagnosticsThread_.join();
    }
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
    
    Logger::GetInstance().Info("AgentDaemon stopped successfully.");
}

bool AgentDaemon::IsRunning() const {
    return running_;
}

bool AgentDaemon::ValidateConfig(const Domain::Models::AgentConfig& config) {
    using namespace Infrastructure::Logging;

    if (config.server_url.empty()) {
        Logger::GetInstance().Error("Config Validation: Server URL is empty.");
        return false;
    }

    // Must be HTTPS
    std::string lowerUrl = config.server_url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    if (lowerUrl.rfind("https://", 0) != 0) {
        Logger::GetInstance().Error("Config Validation: Enforced HTTPS policy failed. Server URL must start with https://. Config URL: " + config.server_url);
        return false;
    }

    if (config.device_id <= 0) {
        Logger::GetInstance().Error("Config Validation: Invalid Device ID: " + std::to_string(config.device_id));
        return false;
    }

    if (config.agent_secret.empty()) {
        Logger::GetInstance().Error("Config Validation: Agent secret key is empty.");
        return false;
    }

    return true;
}

void AgentDaemon::TriggerReconnect() {
    cvReconnect_.notify_all();
}

void AgentDaemon::FlushOfflineCache(const Domain::Models::AgentConfig& config) {
    using namespace Infrastructure::Logging;

    size_t cacheSize = offlineCache_->Size();
    if (cacheSize == 0) return;

    Logger::GetInstance().Info("OfflineCache: Flashing " + std::to_string(cacheSize) + " cached metrics to the server...");

    auto cachedMetrics = offlineCache_->PopAll();
    std::vector<Domain::Models::MetricData> failedSends;

    for (const auto& metrics : cachedMetrics) {
        // If we lost connection during flush, cache the rest immediately and stop
        if (!connected_) {
            failedSends.push_back(metrics);
            continue;
        }

        bool success = httpSender_->SendMetrics(config.server_url, config.agent_secret, metrics);
        if (!success) {
            Logger::GetInstance().Warning("OfflineCache: Failed to send cached metric. Re-caching remaining items.");
            connected_ = false;
            failedSends.push_back(metrics);
            TriggerReconnect();
        }
    }

    // Re-cache anything that failed to send
    if (!failedSends.empty()) {
        for (const auto& metrics : failedSends) {
            offlineCache_->Push(metrics);
        }
    } else {
        Logger::GetInstance().Info("OfflineCache: All cached metrics uploaded successfully.");
    }
}

void AgentDaemon::MetricsLoop() {
    using namespace Infrastructure::Logging;
    using namespace std::chrono_literals;

    while (running_) {
        Domain::Models::AgentConfig config;
        
        if (!configRepo_->Load(config)) {
            Logger::GetInstance().Error("AgentDaemon (Metrics): Failed to load config file.");
        } else if (ValidateConfig(config)) {
            // Collect system metrics
            Domain::Models::MetricData metrics = metricCollector_->Collect();

            if (connected_) {
                bool success = httpSender_->SendMetrics(config.server_url, config.agent_secret, metrics);
                if (success) {
                    // Reset backoff state
                    backoffSeconds_ = 2;
                    consecutiveFailures_ = 0;
                    FlushOfflineCache(config);
                } else {
                    Logger::GetInstance().Warning("AgentDaemon (Metrics): Send failed. Entering disconnected mode.");
                    connected_ = false;
                    offlineCache_->Push(metrics);
                    TriggerReconnect();
                }
            } else {
                // Network is offline; write metrics to local cache
                offlineCache_->Push(metrics);
            }
        }

        // Wait 5 seconds or until woke up on stop signal
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 5s, [this]() { return !running_; });
    }
}

void AgentDaemon::HeartbeatLoop() {
    using namespace Infrastructure::Logging;
    using namespace std::chrono_literals;

    while (running_) {
        // Sleep first, or immediately heartbeat? Heartbeat immediately on startup
        Domain::Models::AgentConfig config;
        
        if (configRepo_->Load(config) && ValidateConfig(config)) {
            if (connected_) {
                bool success = httpSender_->SendHeartbeat(config.server_url, config.agent_secret);
                if (!success) {
                    Logger::GetInstance().Warning("AgentDaemon (Heartbeat): Ping failed. Entering disconnected mode.");
                    connected_ = false;
                    TriggerReconnect();
                }
            }
        }

        // Wait 30 seconds
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 30s, [this]() { return !running_; });
    }
}

void AgentDaemon::ReconnectLoop() {
    using namespace Infrastructure::Logging;
    using namespace std::chrono_literals;

    while (running_) {
        // Wait until disconnected
        std::unique_lock<std::mutex> lock(mutex_);
        cvReconnect_.wait(lock, [this]() { return !connected_ || !running_; });

        if (!running_) break;

        // Release mutex lock during sleep and reconnect work
        lock.unlock();

        int sleepTime = backoffSeconds_;
        Logger::GetInstance().Info("ReconnectLoop: Offline. Retrying heartbeat connection in " + std::to_string(sleepTime) + " seconds (failures: " + std::to_string(consecutiveFailures_.load()) + ")");
        
        // Wait sleepTime seconds while checking for exit signal
        std::unique_lock<std::mutex> sleepLock(mutex_);
        cv_.wait_for(sleepLock, std::chrono::seconds(sleepTime), [this]() { return !running_; });
        if (!running_) break;
        sleepLock.unlock();

        // Load config and attempt reconnect
        Domain::Models::AgentConfig config;
        if (configRepo_->Load(config) && ValidateConfig(config)) {
            bool success = httpSender_->SendHeartbeat(config.server_url, config.agent_secret);
            if (success) {
                Logger::GetInstance().Info("ReconnectLoop: Network connection restored!");
                connected_ = true;
                backoffSeconds_ = 2;
                consecutiveFailures_ = 0;
                FlushOfflineCache(config);
            } else {
                consecutiveFailures_++;
                // Double backoff up to 60s
                backoffSeconds_ = std::min(60, backoffSeconds_ * 2);
            }
        } else {
            // Configuration became invalid
            consecutiveFailures_++;
            backoffSeconds_ = std::min(60, backoffSeconds_ * 2);
        }
    }
}

bool AgentDaemon::IsNewerVersion(const std::string& current, const std::string& latest) {
    auto parseVersion = [](const std::string& v, int& major, int& minor, int& patch) {
        major = 0; minor = 0; patch = 0;
        std::stringstream ss(v);
        char dot1 = '\0', dot2 = '\0';
        ss >> major >> dot1 >> minor >> dot2 >> patch;
    };

    int curMajor = 0, curMinor = 0, curPatch = 0;
    int latMajor = 0, latMinor = 0, latPatch = 0;
    parseVersion(current, curMajor, curMinor, curPatch);
    parseVersion(latest, latMajor, latMinor, latPatch);

    if (latMajor != curMajor) return latMajor > curMajor;
    if (latMinor != curMinor) return latMinor > curMinor;
    return latPatch > curPatch;
}

std::string AgentDaemon::CalculateFileSHA256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return "";

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result = "";

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        DWORD cbHashObject = 0, cbData = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) == 0) {
            std::vector<BYTE> hashObject(cbHashObject);
            if (BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, nullptr, 0, 0) == 0) {
                std::vector<char> buffer(16384);
                while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
                    BCryptHashData(hHash, (PBYTE)buffer.data(), static_cast<ULONG>(file.gcount()), 0);
                }

                DWORD cbHash = 0;
                if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) == 0) {
                    std::vector<BYTE> hashVal(cbHash);
                    if (BCryptFinishHash(hHash, hashVal.data(), cbHash, 0) == 0) {
                        std::stringstream ss;
                        for (BYTE b : hashVal) {
                            ss << std::hex << std::setfill('0') << std::setw(2) << (int)b;
                        }
                        result = ss.str();
                    }
                }
            }
        }
    }

    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

void AgentDaemon::ExecuteUpdaterScript() {
    using namespace Infrastructure::Logging;

    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
        Logger::GetInstance().Error("ExecuteUpdaterScript: Failed to resolve current module path.");
        return;
    }
    std::filesystem::path installDir = std::filesystem::path(modulePath).parent_path();

    std::filesystem::path batPath = installDir / "update.bat";
    std::filesystem::path exePath = installDir / "ProactiveITAgent.exe";
    std::filesystem::path newPath = installDir / "ProactiveITAgent.new";
    std::filesystem::path bakPath = installDir / "ProactiveITAgent.bak";

    std::ofstream ofs(batPath);
    if (!ofs.is_open()) {
        Logger::GetInstance().Error("ExecuteUpdaterScript: Failed to create update.bat");
        return;
    }

    ofs << "@echo off\n"
        << ":: Wait for main service process to release locks and exit\n"
        << "ping 127.0.0.1 -n 3 > nul\n"
        << "sc stop ProactiveITAgent > nul\n"
        << "ping 127.0.0.1 -n 2 > nul\n"
        << "if exist \"" << bakPath.string() << "\" del /f /q \"" << bakPath.string() << "\"\n"
        << "rename \"" << exePath.string() << "\" \"ProactiveITAgent.bak\"\n"
        << "move /y \"" << newPath.string() << "\" \"" << exePath.string() << "\"\n"
        << "sc start ProactiveITAgent > nul\n"
        << "ping 127.0.0.1 -n 5 > nul\n"
        << "sc query ProactiveITAgent | findstr RUNNING > nul\n"
        << "if %errorlevel% equ 0 (\n"
        << "    del /f /q \"" << bakPath.string() << "\"\n"
        << "    exit /b 0\n"
        << ") else (\n"
        << "    sc stop ProactiveITAgent > nul\n"
        << "    del /f /q \"" << exePath.string() << "\"\n"
        << "    rename \"" << bakPath.string() << "\" \"ProactiveITAgent.exe\"\n"
        << "    sc start ProactiveITAgent > nul\n"
        << "    exit /b 1\n"
        << ")\n";
    ofs.close();

    Logger::GetInstance().Info("ExecuteUpdaterScript: update.bat written successfully. Spawning detached process...");

    std::wstring cmdLine = L"cmd.exe /c \"" + batPath.wstring() + L"\"";
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(L'\0');

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessW(
        nullptr,
        cmdLineBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | DETACHED_PROCESS,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (success) {
        Logger::GetInstance().Info("ExecuteUpdaterScript: Detached updater process spawned. Stopping service loop.");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        running_ = false;
        cv_.notify_all();
    } else {
        Logger::GetInstance().Error("ExecuteUpdaterScript: Failed to spawn detached updater process. Error code: " + std::to_string(GetLastError()));
    }
}

void AgentDaemon::UpdateLoop() {
    using namespace Infrastructure::Logging;
    using namespace std::chrono_literals;

    // Wait 2 minutes on startup before performing first version check
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 2min, [this]() { return !running_; });
    }

    const std::string CURRENT_VERSION = "2.0.0";

    while (running_) {
        Domain::Models::AgentConfig config;
        if (configRepo_->Load(config) && ValidateConfig(config)) {
            if (connected_) {
                std::string latestVer, downloadUrl, expectedChecksum;
                bool success = httpSender_->GetLatestAgentInfo(config.server_url, latestVer, downloadUrl, expectedChecksum);
                if (success) {
                    if (IsNewerVersion(CURRENT_VERSION, latestVer)) {
                        Logger::GetInstance().Info("UpdateLoop: Newer agent version found: " + latestVer + " (current: " + CURRENT_VERSION + "). Initializing download...");
                        
                        wchar_t modulePath[MAX_PATH];
                        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) != 0) {
                            std::filesystem::path installDir = std::filesystem::path(modulePath).parent_path();
                            std::filesystem::path newExePath = installDir / "ProactiveITAgent.new";

                            if (httpSender_->DownloadFile(downloadUrl, newExePath.string())) {
                                Logger::GetInstance().Info("UpdateLoop: Download complete. Calculating checksum...");
                                std::string actualChecksum = CalculateFileSHA256(newExePath.string());

                                std::string expChecksum = expectedChecksum;
                                std::transform(expChecksum.begin(), expChecksum.end(), expChecksum.begin(), [](unsigned char c) {
                                    return std::tolower(c);
                                });

                                if (actualChecksum == expChecksum) {
                                    Logger::GetInstance().Info("UpdateLoop: Checksum verified successfully. Starting update replacement process...");
                                    ExecuteUpdaterScript();
                                    break;
                                } else {
                                    Logger::GetInstance().Error("UpdateLoop: Checksum mismatch. Expected: " + expChecksum + ", Calculated: " + actualChecksum + ". Deleting download.");
                                    std::error_code ec;
                                    std::filesystem::remove(newExePath, ec);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Wait 15 minutes
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 15min, [this]() { return !running_; });
    }
}

void AgentDaemon::WritePowerShellScriptIfMissing() {
    using namespace Infrastructure::Logging;
    
    std::string scriptPath = "collect_diagnostics.ps1";
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) != 0) {
        std::filesystem::path installDir = std::filesystem::path(modulePath).parent_path();
        std::filesystem::path fullPath = installDir / "collect_diagnostics.ps1";
        scriptPath = fullPath.string();
    }

    Logger::GetInstance().Info("Writing diagnostics collector script to: " + scriptPath);
    
    std::ofstream ofs(scriptPath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        Logger::GetInstance().Error("Failed to write collect_diagnostics.ps1 to path: " + scriptPath);
        return;
    }

    const char* scriptContent = R"raw(param(
    [string]$Type = "Dynamic",
    [int]$LastLogMinutes = 60,
    [string]$ServerUrl = ""
)

# Set UTF-8 output
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if ($Type -eq "Static") {
    # System Info
    $sys = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
    $bios = Get-CimInstance Win32_Bios -ErrorAction SilentlyContinue
    $os = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
    
    $systemInfo = @{
        manufacturer = if ($sys) { $sys.Manufacturer } else { "Unknown" }
        model = if ($sys) { $sys.Model } else { "Unknown" }
        serial_number = if ($bios) { $bios.SerialNumber } else { "Unknown" }
        bios_version = if ($bios) { $bios.SMBIOSBIOSVersion } else { "Unknown" }
        windows_edition = if ($os) { $os.Caption } else { "Unknown" }
        windows_build = if ($os) { $os.Version } else { "Unknown" }
        architecture = if ($os) { $os.OSArchitecture } else { "Unknown" }
        last_boot_time = if ($os -and $os.LastBootUpTime) { $os.LastBootUpTime.ToString("yyyy-MM-dd HH:mm:ss") } else { "Unknown" }
        logged_in_user = if ($sys) { $sys.UserName } else { "Unknown" }
    }

    # Installed Software
    $paths = @(
        "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    $installedSoftware = @()
    try {
        $installedSoftware = Get-ItemProperty $paths -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -and $_.SystemComponent -ne 1 -and $_.ParentKeyName -eq $null } |
            ForEach-Object {
                [PSCustomObject]@{
                    name = $_.DisplayName
                    version = if ($_.DisplayVersion) { $_.DisplayVersion.ToString() } else { "Unknown" }
                    install_date = if ($_.InstallDate) { $_.InstallDate.ToString() } else { "Unknown" }
                }
            } | Sort-Object name
    } catch {}

    # Startup Applications
    $startup = @()
    $runPaths = @("HKLM:\Software\Microsoft\Windows\CurrentVersion\Run", "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run")
    foreach ($rp in $runPaths) {
        if (Test-Path $rp) {
            $valNames = Get-Item -Path $rp | Select-Object -ExpandProperty Property -ErrorAction SilentlyContinue
            foreach ($vn in $valNames) {
                $val = Get-ItemPropertyValue -Path $rp -Name $vn -ErrorAction SilentlyContinue
                $startup += [PSCustomObject]@{ name = $vn; path = $val; location = "Registry ($rp)" }
            }
        }
    }
    $commonStartup = [Environment]::GetFolderPath("CommonStartup")
    $userStartup = [Environment]::GetFolderPath("Startup")
    if (Test-Path $commonStartup) {
        Get-ChildItem $commonStartup -ErrorAction SilentlyContinue | ForEach-Object {
            $startup += [PSCustomObject]@{ name = $_.Name; path = $_.FullName; location = "Common Startup Folder" }
        }
    }
    if (Test-Path $userStartup) {
        Get-ChildItem $userStartup -ErrorAction SilentlyContinue | ForEach-Object {
            $startup += [PSCustomObject]@{ name = $_.Name; path = $_.FullName; location = "User Startup Folder" }
        }
    }

    $result = @{
        system_info = $systemInfo
        installed_software = $installedSoftware
        startup_applications = $startup
    }
    
    $result | ConvertTo-Json -Depth 5
}
else {
    # Dynamic Diagnostics
    
    # 1. CPU Info
    $cpuFreq = 0
    try {
        $cpuFreq = (Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue).CurrentClockSpeed
    } catch {}
    
    $cores = @()
    try {
        $cores = Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -ErrorAction SilentlyContinue | 
            Where-Object { $_.Name -ne "_Total" } | 
            ForEach-Object { [double]$_.PercentProcessorTime }
    } catch {}
    
    $topCpu = @()
    try {
        $topCpu = Get-CimInstance Win32_PerfFormattedData_PerfProc_Process -ErrorAction SilentlyContinue | 
            Where-Object { $_.Name -ne "_Total" -and $_.Name -ne "Idle" } |
            Sort-Object PercentProcessorTime -Descending | Select-Object -First 10 |
            ForEach-Object { [PSCustomObject]@{ name = $_.Name; pid = $_.IDProcess; cpu_usage = [double]$_.PercentProcessorTime } }
    } catch {}
    
    $cpuInfo = @{
        frequency_mhz = $cpuFreq
        cores_usage = $cores
        top_processes = $topCpu
    }

    # 2. Memory Info
    $totalRam = 0
    $usedRam = 0
    $freeRam = 0
    $ramPercent = 0
    try {
        $osMem = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
        if ($osMem) {
            $totalRam = $osMem.TotalVisibleMemorySize * 1024
            $freeRam = $osMem.FreePhysicalMemory * 1024
            $usedRam = $totalRam - $freeRam
            $ramPercent = if ($totalRam -gt 0) { ($usedRam / $totalRam) * 100 } else { 0 }
        }
    } catch {}
    
    $topMem = @()
    try {
        $topMem = Get-Process -ErrorAction SilentlyContinue | Sort-Object WorkingSet64 -Descending | Select-Object -First 10 |
            ForEach-Object { [PSCustomObject]@{ name = $_.ProcessName; pid = $_.Id; memory_usage_mb = [math]::Round($_.WorkingSet64 / 1MB, 1) } }
    } catch {}
    
    $memoryInfo = @{
        total_ram_mb = [math]::Round($totalRam / 1MB, 1)
        used_ram_mb = [math]::Round($usedRam / 1MB, 1)
        available_ram_mb = [math]::Round($freeRam / 1MB, 1)
        utilization_percent = [math]::Round($ramPercent, 1)
        top_processes = $topMem
    }

    # 3. Storage Info
    $storageInfo = @()
    try {
        $physicalDisks = Get-PhysicalDisk -ErrorAction SilentlyContinue
        $drives = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=3" -ErrorAction SilentlyContinue
        foreach ($d in $drives) {
            $total = $d.Size
            $free = $d.FreeSpace
            $used = $total - $free
            $pct = if ($total -gt 0) { ($used / $total) * 100 } else { 0 }
            
            $smart = "Healthy"
            $type = "HDD"
            $healthStatus = "Healthy"
            if ($physicalDisks) {
                $smart = $physicalDisks[0].OperationalStatus -join ", "
                $type = $physicalDisks[0].MediaType.ToString()
                $healthStatus = $physicalDisks[0].HealthStatus.ToString()
            }
            
            $storageInfo += [PSCustomObject]@{
                drive_letter = $d.DeviceID
                drive_type = $type
                total_capacity_gb = [math]::Round($total / 1GB, 1)
                used_capacity_gb = [math]::Round($used / 1GB, 1)
                free_capacity_gb = [math]::Round($free / 1GB, 1)
                percent_used = [math]::Round($pct, 1)
                smart_status = $smart
                disk_health_indicators = $healthStatus
            }
        }
    } catch {}

    # 4. Battery Info
    $batteryAvailable = $false
    $batteryPct = 100
    $chargingStatus = "AC Power"
    $estTime = -1
    $batteryHealth = "Healthy"
    try {
        $battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue
        if ($battery) {
            $batteryAvailable = $true
            $batteryPct = $battery.EstimatedChargeRemaining
            $estTime = $battery.EstimatedRunTime
            $chargingStatus = switch ($battery.BatteryStatus) {
                1 { "Discharging" }
                2 { "AC Power" }
                3 { "Fully Charged" }
                10 { "Charging" }
                default { "Unknown" }
            }
            $batteryHealth = $battery.Status
        }
    } catch {}
    
    $batteryInfo = @{
        battery_available = $batteryAvailable
        percentage = $batteryPct
        charging_status = $chargingStatus
        estimated_remaining_time_mins = $estTime
        battery_health = $batteryHealth
    }

    # 5. Network Info
    $localIp = "127.0.0.1"
    $publicIp = "Unknown"
    $macAddress = "00:00:00:00:00:00"
    $hostname = [System.Net.Dns]::GetHostName()
    $gateway = "Unknown"
    $dnsServers = @()
    $adapterName = "None"
    $connType = "Unknown"
    $internetStatus = "Disconnected"
    $latency = -1
    
    try {
        $adapter = Get-NetAdapter -ErrorAction SilentlyContinue | Where-Object { $_.Status -eq 'Up' } | Select-Object -First 1
        if ($adapter) {
            $adapterName = $adapter.Name
            $macAddress = $adapter.MacAddress
            if ($adapter.InterfaceDescription -like "*Wi-Fi*" -or $adapter.InterfaceDescription -like "*Wireless*") {
                $connType = "Wi-Fi"
            } elseif ($adapter.InterfaceDescription -like "*Ethernet*") {
                $connType = "Ethernet"
            }
        }
        
        $routes = Get-NetRoute -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue
        if ($routes) {
            $gateway = $routes[0].NextHop
        }
        
        $dns = Get-DnsClientServerAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.ServerAddresses }
        if ($dns) {
            $dnsServers = $dns.ServerAddresses
        }
        
        $ipConf = Get-NetIPAddress -AddressFamily IPv4 -InterfaceIndex (if ($adapter) { $adapter.InterfaceIndex } else { 0 }) -ErrorAction SilentlyContinue
        if ($ipConf) {
            $localIp = $ipConf[0].IPAddress
        }

        # Public IP
        $publicIp = Invoke-RestMethod -Uri "https://api.ipify.org" -TimeoutSec 2 -ErrorAction SilentlyContinue
        if (!$publicIp) { $publicIp = "Unknown" }
        
        # Internet status
        $pingGoogle = Test-Connection -ComputerName 8.8.8.8 -Count 1 -TimeoutMilliSec 1000 -ErrorAction SilentlyContinue
        if ($pingGoogle) {
            $internetStatus = "Connected"
        }
        
        # Latency to backend
        if ($ServerUrl) {
            try {
                $uri = [System.Uri]$ServerUrl
                $pingBackend = Test-Connection -ComputerName $uri.Host -Count 1 -TimeoutMilliSec 1000 -ErrorAction SilentlyContinue
                if ($pingBackend) {
                    $latency = $pingBackend.ResponseTime
                }
            } catch {}
        }
    } catch {}
    
    $networkInfo = @{
        local_ip = $localIp
        public_ip = $publicIp
        mac_address = $macAddress
        hostname = $hostname
        default_gateway = $gateway
        dns_servers = $dnsServers
        active_adapter = $adapterName
        connection_type = $connType
        internet_status = $internetStatus
        latency_ms = $latency
        upload_throughput_mbps = 0.0
        download_throughput_mbps = 0.0
    }

    # 6. Security Info
    $defenderStatus = "Unknown"
    $firewallStatus = "Unknown"
    $updateStatus = "Unknown"
    $pendingRestart = $false
    $bitlockerStatus = "Unknown"
    try {
        $defender = Get-MpComputerStatus -ErrorAction SilentlyContinue
        if ($defender) {
            $defenderStatus = if ($defender.RealTimeProtectionEnabled) { "Active" } else { "Disabled" }
        } else {
            $svc = Get-Service -Name WinDefend -ErrorAction SilentlyContinue
            $defenderStatus = if ($svc) { if ($svc.Status -eq "Running") { "Active" } else { "Disabled" } } else { "Unknown" }
        }
        
        $fw = Get-NetFirewallProfile -ErrorAction SilentlyContinue
        if ($fw) {
            $fwStatus = if (($fw | Where-Object { $_.Enabled -eq $true }).Count -eq $fw.Count) { "Active" } else { "Degraded/Disabled" }
            $firewallStatus = $fwStatus
        }
        
        $svcUpdate = Get-Service -Name wuauserv -ErrorAction SilentlyContinue
        $updateStatus = if ($svcUpdate) { $svcUpdate.Status.ToString() } else { "Unknown" }
        
        if (Test-Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired") { $pendingRestart = $true }
        if (Test-Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending") { $pendingRestart = $true }
        
        $bitlocker = Get-BitLockerVolume -MountPoint "C:" -ErrorAction SilentlyContinue
        if ($bitlocker) {
            $bitlockerStatus = $bitlocker.VolumeStatus.ToString()
        }
    } catch {}
    
    $securityInfo = @{
        windows_defender = $defenderStatus
        firewall_status = $firewallStatus
        windows_update_status = $updateStatus
        pending_restart = $pendingRestart
        bitlocker_status = $bitlockerStatus
    }

    # 7. Processes and Services
    $processCount = 0
    try {
        $processCount = (Get-Process -ErrorAction SilentlyContinue).Count
    } catch {}
    
    $servicesToMonitor = @("WinDefend", "MpsSvc", "wuauserv", "Dhcp", "Dnscache", "Spooler", "EventLog")
    $criticalServices = @()
    $failedStoppedServices = @()
    try {
        $criticalServices = Get-Service -Name $servicesToMonitor -ErrorAction SilentlyContinue | ForEach-Object {
            [PSCustomObject]@{
                name = $_.Name
                display_name = $_.DisplayName
                status = $_.Status.ToString()
                start_type = $_.StartType.ToString()
            }
        }
        $failedStoppedServices = Get-Service -Name $servicesToMonitor -ErrorAction SilentlyContinue | 
            Where-Object { $_.Status -ne "Running" -and $_.StartType -eq "Automatic" } | 
            ForEach-Object { $_.Name }
    } catch {}
    
    $processesInfo = @{
        running_process_count = $processCount
        top_cpu_processes = $topCpu
        top_memory_processes = $topMem
        critical_services = $criticalServices
        failed_stopped_critical_services = $failedStoppedServices
    }

    # 8. Event Logs (Recent Warning/Error/Critical)
    $since = (Get-Date).AddMinutes(-$LastLogMinutes)
    $events = @()
    try {
        $events += Get-WinEvent -FilterHashtable @{LogName='System'; Level=@(1,2,3); StartTime=$since} -MaxEvents 30 -ErrorAction SilentlyContinue | ForEach-Object {
            [PSCustomObject]@{
                log_name = "System"
                event_id = $_.Id
                source = $_.ProviderName
                type = $_.LevelDisplayName
                time_created = $_.TimeCreated.ToString("yyyy-MM-dd HH:mm:ss")
                message = $_.Message
            }
        }
    } catch {}
    try {
        $events += Get-WinEvent -FilterHashtable @{LogName='Application'; Level=@(1,2,3); StartTime=$since} -MaxEvents 30 -ErrorAction SilentlyContinue | ForEach-Object {
            [PSCustomObject]@{
                log_name = "Application"
                event_id = $_.Id
                source = $_.ProviderName
                type = $_.LevelDisplayName
                time_created = $_.TimeCreated.ToString("yyyy-MM-dd HH:mm:ss")
                message = $_.Message
            }
        }
    } catch {}
    try {
        $events += Get-WinEvent -FilterHashtable @{LogName='System'; Id=@(1001, 6008); StartTime=$since} -ErrorAction SilentlyContinue | ForEach-Object {
            [PSCustomObject]@{
                log_name = "System"
                event_id = $_.Id
                source = $_.ProviderName
                type = "Critical"
                time_created = $_.TimeCreated.ToString("yyyy-MM-dd HH:mm:ss")
                message = $_.Message
            }
        }
    } catch {}

    $result = @{
        cpu_info = $cpuInfo
        memory_info = $memoryInfo
        storage_info = $storageInfo
        battery_info = $batteryInfo
        network_info = $networkInfo
        security_info = $securityInfo
        processes_info = $processesInfo
        event_logs = $events
    }
    
    $result | ConvertTo-Json -Depth 5
})raw";

    ofs.write(scriptContent, strlen(scriptContent));
    ofs.close();
}

std::string AgentDaemon::ExecutePowerShell(const std::wstring& args) {
    using namespace Infrastructure::Logging;
    
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass " + args;
    
    FILE* pipe = _wpopen(cmd.c_str(), L"r");
    if (!pipe) {
        Logger::GetInstance().Error("ExecutePowerShell: Failed to spawn process.");
        return "";
    }
    
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    _pclose(pipe);
    return result;
}

void AgentDaemon::DiagnosticsLoop() {
    using namespace Infrastructure::Logging;
    using namespace std::chrono_literals;

    WritePowerShellScriptIfMissing();

    std::string scriptPath = "collect_diagnostics.ps1";
    wchar_t modulePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) != 0) {
        std::filesystem::path installDir = std::filesystem::path(modulePath).parent_path();
        scriptPath = (installDir / "collect_diagnostics.ps1").string();
    }
    
    std::wstring wScriptPath = std::wstring(scriptPath.begin(), scriptPath.end());
    
    bool initialSend = true;
    auto lastStaticSend = std::chrono::steady_clock::now() - std::chrono::hours(25);

    // Sleep for 5 seconds on startup to let system settle before diagnostics run
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 5s, [this]() { return !running_; });
    }

    while (running_) {
        Domain::Models::AgentConfig config;
        
        if (configRepo_->Load(config) && ValidateConfig(config)) {
            if (connected_) {
                auto now = std::chrono::steady_clock::now();
                bool sendStatic = initialSend || (std::chrono::duration_cast<std::chrono::hours>(now - lastStaticSend).count() >= 24);
                
                std::wstring serverUrlW = std::wstring(config.server_url.begin(), config.server_url.end());
                std::wstring dynamicArgs = L"-File \"" + wScriptPath + L"\" -Type Dynamic -LastLogMinutes 2 -ServerUrl \"" + serverUrlW + L"\"";
                
                Logger::GetInstance().Info("DiagnosticsLoop: Running dynamic diagnostics script...");
                std::string dynamicJson = ExecutePowerShell(dynamicArgs);
                
                if (!dynamicJson.empty()) {
                    bool success = httpSender_->SendDiagnostics(config.server_url, config.agent_secret, dynamicJson);
                    if (success) {
                        Logger::GetInstance().Info("DiagnosticsLoop: Dynamic diagnostics uploaded successfully.");
                    } else {
                        Logger::GetInstance().Warning("DiagnosticsLoop: Dynamic diagnostics upload failed.");
                    }
                }
                
                if (sendStatic) {
                    std::wstring staticArgs = L"-File \"" + wScriptPath + L"\" -Type Static";
                    Logger::GetInstance().Info("DiagnosticsLoop: Running static diagnostics script...");
                    std::string staticJson = ExecutePowerShell(staticArgs);
                    
                    if (!staticJson.empty()) {
                        bool success = httpSender_->SendDiagnostics(config.server_url, config.agent_secret, staticJson);
                        if (success) {
                            Logger::GetInstance().Info("DiagnosticsLoop: Static diagnostics uploaded successfully.");
                            lastStaticSend = now;
                            initialSend = false;
                        } else {
                            Logger::GetInstance().Warning("DiagnosticsLoop: Static diagnostics upload failed.");
                        }
                    }
                }
            }
        }
        
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 60s, [this]() { return !running_; });
    }
}

} // namespace Application
