#include "Infrastructure/Metrics/WindowsMetricCollector.h"
#include "Infrastructure/Logging/Logger.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wtsapi32.h>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace Infrastructure::Metrics {

WindowsMetricCollector::WindowsMetricCollector()
    : cpuInitialized_(false) {
    prevIdleTime_ = {0, 0};
    prevKernelTime_ = {0, 0};
    prevUserTime_ = {0, 0};
}

Domain::Models::MetricData WindowsMetricCollector::Collect() {
    std::lock_guard<std::mutex> lock(mutex_);
    Domain::Models::MetricData data;
    
    data.cpu_usage = CollectCpuUsage();
    data.ram_usage = CollectRamUsage();
    data.disk_usage = CollectDiskUsage();
    data.uptime = CollectUptime();
    
    CollectNetworkUsage(data.network_in, data.network_out);
    
    data.hostname = CollectHostname();
    data.username = CollectLoggedUsername();
    data.windows_version = CollectWindowsVersion();
    
    CollectNetworkDetails(data.mac_address, data.local_ip);
    
    data.machine_guid = CollectMachineGuid();

    return data;
}

ULONGLONG WindowsMetricCollector::SubtractFileTimes(const FILETIME& ftA, const FILETIME& ftB) {
    ULARGE_INTEGER a, b;
    a.LowPart = ftA.dwLowDateTime;
    a.HighPart = ftA.dwHighDateTime;
    b.LowPart = ftB.dwLowDateTime;
    b.HighPart = ftB.dwHighDateTime;
    return a.QuadPart - b.QuadPart;
}

double WindowsMetricCollector::CollectCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        Logging::Logger::GetInstance().Error("Failed to get system times for CPU calculation.");
        return 0.0;
    }

    if (!cpuInitialized_) {
        prevIdleTime_ = idleTime;
        prevKernelTime_ = kernelTime;
        prevUserTime_ = userTime;
        cpuInitialized_ = true;
        
        // Wait briefly on first collect to get a delta
        Sleep(100);
        
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return 0.0;
        }
    }

    ULONGLONG idleDiff = SubtractFileTimes(idleTime, prevIdleTime_);
    ULONGLONG kernelDiff = SubtractFileTimes(kernelTime, prevKernelTime_);
    ULONGLONG userDiff = SubtractFileTimes(userTime, prevUserTime_);

    prevIdleTime_ = idleTime;
    prevKernelTime_ = kernelTime;
    prevUserTime_ = userTime;

    ULONGLONG totalSystemTime = kernelDiff + userDiff;
    if (totalSystemTime == 0) {
        return 0.0;
    }

    // Kernel time includes idle time, so (total - idle) is the busy time
    ULONGLONG actualSystemTime = totalSystemTime - idleDiff;
    double cpuPercentage = (double(actualSystemTime) / double(totalSystemTime)) * 100.0;

    if (cpuPercentage < 0.0) cpuPercentage = 0.0;
    if (cpuPercentage > 100.0) cpuPercentage = 100.0;

    return cpuPercentage;
}

double WindowsMetricCollector::CollectRamUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.dwMemoryLoad);
    }
    Logging::Logger::GetInstance().Error("Failed to query global memory status.");
    return 0.0;
}

double WindowsMetricCollector::CollectDiskUsage() {
    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFreeBytes)) {
        double total = static_cast<double>(totalBytes.QuadPart);
        double free = static_cast<double>(totalFreeBytes.QuadPart);
        if (total > 0.0) {
            double used = total - free;
            return (used / total) * 100.0;
        }
    }
    Logging::Logger::GetInstance().Error("Failed to query disk space for C:\\.");
    return 0.0;
}

long long WindowsMetricCollector::CollectUptime() {
    return static_cast<long long>(GetTickCount64() / 1000);
}

void WindowsMetricCollector::CollectNetworkUsage(double& networkIn, double& networkOut) {
    networkIn = 0.0;
    networkOut = 0.0;
    
    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; ++i) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK && row.OperStatus == IfOperStatusUp) {
                networkIn += static_cast<double>(row.InOctets);
                networkOut += static_cast<double>(row.OutOctets);
            }
        }
        FreeMibTable(ifTable);
    } else {
        Logging::Logger::GetInstance().Error("Failed to get network interface table for traffic stats.");
    }
    
    // Convert bytes to MB
    networkIn = networkIn / (1024.0 * 1024.0);
    networkOut = networkOut / (1024.0 * 1024.0);
}

std::string WindowsMetricCollector::CollectHostname() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = ARRAYSIZE(buffer);
    if (GetComputerNameW(buffer, &size)) {
        return WstringToUtf8(buffer);
    }
    return "UnknownHost";
}

std::string WindowsMetricCollector::CollectLoggedUsername() {
    WTS_SESSION_INFOW* sessionInfo = nullptr;
    DWORD count = 0;
    std::string result = "None";
    
    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo, &count)) {
        for (DWORD i = 0; i < count; ++i) {
            if (sessionInfo[i].State == WTSActive) {
                LPWSTR username = nullptr;
                DWORD bytesReturned = 0;
                if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionInfo[i].SessionId, WTSUserName, &username, &bytesReturned)) {
                    std::wstring wuser(username);
                    WTSFreeMemory(username);
                    if (!wuser.empty()) {
                        result = WstringToUtf8(wuser);
                        break;
                    }
                }
            }
        }
        WTSFreeMemory(sessionInfo);
    }
    return result;
}

std::string WindowsMetricCollector::CollectWindowsVersion() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t productName[256] = {0};
        wchar_t displayVersion[64] = {0};
        wchar_t currentBuild[64] = {0};
        DWORD sizeProduct = sizeof(productName);
        DWORD sizeDisplay = sizeof(displayVersion);
        DWORD sizeBuild = sizeof(currentBuild);

        RegQueryValueExW(hKey, L"ProductName", nullptr, nullptr, (LPBYTE)productName, &sizeProduct);
        RegQueryValueExW(hKey, L"DisplayVersion", nullptr, nullptr, (LPBYTE)displayVersion, &sizeDisplay);
        RegQueryValueExW(hKey, L"CurrentBuild", nullptr, nullptr, (LPBYTE)currentBuild, &sizeBuild);
        RegCloseKey(hKey);

        std::wstring versionStr = productName;
        if (displayVersion[0] != L'\0') {
            versionStr += L" " + std::wstring(displayVersion);
        }
        if (currentBuild[0] != L'\0') {
            versionStr += L" (Build " + std::wstring(currentBuild) + L")";
        }
        return WstringToUtf8(versionStr);
    }
    return "Windows Unknown";
}

void WindowsMetricCollector::CollectNetworkDetails(std::string& macAddress, std::string& localIp) {
    macAddress = "00:00:00:00:00:00";
    localIp = "127.0.0.1";
    
    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES addresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    if (addresses == nullptr) return;

    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, addresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    }
    
    if (addresses != nullptr) {
        bool macFound = false;
        bool ipFound = false;
        
        for (PIP_ADAPTER_ADDRESSES curr = addresses; curr != nullptr; curr = curr->Next) {
            if (curr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (curr->OperStatus != IfOperStatusUp) continue;
            
            // Get MAC (physical) address of first active adapter
            if (!macFound && curr->PhysicalAddressLength >= 6) {
                char buf[32];
                sprintf_s(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
                    curr->PhysicalAddress[0], curr->PhysicalAddress[1],
                    curr->PhysicalAddress[2], curr->PhysicalAddress[3],
                    curr->PhysicalAddress[4], curr->PhysicalAddress[5]);
                macAddress = buf;
                macFound = true;
            }
            
            // Get local IPv4 address
            if (!ipFound) {
                for (PIP_ADAPTER_UNICAST_ADDRESS unicast = curr->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next) {
                    if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                        sockaddr_in* ipv4 = (sockaddr_in*)unicast->Address.lpSockaddr;
                        char ipBuf[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &(ipv4->sin_addr), ipBuf, INET_ADDRSTRLEN) != nullptr) {
                            localIp = ipBuf;
                            ipFound = true;
                            break;
                        }
                    }
                }
            }
            
            if (macFound && ipFound) break;
        }
        free(addresses);
    }
}

std::string WindowsMetricCollector::CollectMachineGuid() {
    HKEY hKey;
    // Registry Key for Machine GUID is in 64-bit hive
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t guid[128] = {0};
        DWORD size = sizeof(guid);
        if (RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, (LPBYTE)guid, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return WstringToUtf8(guid);
        }
        RegCloseKey(hKey);
    }
    return "00000000-0000-0000-0000-000000000000";
}

std::string WindowsMetricCollector::WstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, nullptr, nullptr);
    return strTo;
}

} // namespace Infrastructure::Metrics
