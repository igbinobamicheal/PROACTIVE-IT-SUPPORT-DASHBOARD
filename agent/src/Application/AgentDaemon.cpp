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

        // Wait 15 seconds or until woke up on stop signal
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 15s, [this]() { return !running_; });
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

} // namespace Application
