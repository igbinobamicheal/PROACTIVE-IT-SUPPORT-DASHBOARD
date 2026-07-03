param(
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

        # Public IP with short timeout
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
}
