#include "MetricCollector.h"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <iostream>

#pragma comment(lib, "iphlpapi.lib")

MetricCollector::MetricCollector() : initialized(false) {
    prevIdleTime = {0, 0};
    prevKernelTime = {0, 0};
    prevUserTime = {0, 0};
}

ULONGLONG MetricCollector::subtractFileTimes(const FILETIME& ftA, const FILETIME& ftB) {
    ULARGE_INTEGER a, b;
    a.LowPart = ftA.dwLowDateTime;
    a.HighPart = ftA.dwHighDateTime;
    b.LowPart = ftB.dwLowDateTime;
    b.HighPart = ftB.dwHighDateTime;
    return a.QuadPart - b.QuadPart;
}

double MetricCollector::collectCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        std::cerr << "[MetricCollector] Failed to get system times." << std::endl;
        return 0.0;
    }

    if (!initialized) {
        prevIdleTime = idleTime;
        prevKernelTime = kernelTime;
        prevUserTime = userTime;
        initialized = true;
        
        // Sleep briefly to establish a base delta on first run
        Sleep(100);
        
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return 0.0;
        }
    }

    ULONGLONG idleDiff = subtractFileTimes(idleTime, prevIdleTime);
    ULONGLONG kernelDiff = subtractFileTimes(kernelTime, prevKernelTime);
    ULONGLONG userDiff = subtractFileTimes(userTime, prevUserTime);

    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    ULONGLONG totalSystemTime = kernelDiff + userDiff;
    if (totalSystemTime == 0) {
        return 0.0;
    }

    // Kernel time includes idle time; subtract idle time to obtain actual busy CPU time.
    ULONGLONG actualSystemTime = totalSystemTime - idleDiff;
    double cpuPercentage = (double(actualSystemTime) / double(totalSystemTime)) * 100.0;

    if (cpuPercentage < 0.0) cpuPercentage = 0.0;
    if (cpuPercentage > 100.0) cpuPercentage = 100.0;

    return cpuPercentage;
}

double MetricCollector::collectRamUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return static_cast<double>(memInfo.dwMemoryLoad);
    }
    std::cerr << "[MetricCollector] Failed to get memory status." << std::endl;
    return 0.0;
}

double MetricCollector::collectDiskUsage() {
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        double total = static_cast<double>(totalNumberOfBytes.QuadPart);
        double free = static_cast<double>(totalNumberOfFreeBytes.QuadPart);
        if (total > 0.0) {
            double used = total - free;
            return (used / total) * 100.0;
        }
    }
    std::cerr << "[MetricCollector] Failed to get disk free space for C:\\." << std::endl;
    return 0.0;
}

double MetricCollector::collectNetworkIn() {
    PMIB_IF_TABLE2 ifTable = nullptr;
    double totalInBytes = 0.0;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; ++i) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK && row.OperStatus == IfOperStatusUp) {
                totalInBytes += static_cast<double>(row.InOctets);
            }
        }
        FreeMibTable(ifTable);
    } else {
        std::cerr << "[MetricCollector] Failed to get Network interface table (In)." << std::endl;
    }
    return totalInBytes / (1024.0 * 1024.0); // Convert to MB
}

double MetricCollector::collectNetworkOut() {
    PMIB_IF_TABLE2 ifTable = nullptr;
    double totalOutBytes = 0.0;
    if (GetIfTable2(&ifTable) == NO_ERROR) {
        for (ULONG i = 0; i < ifTable->NumEntries; ++i) {
            MIB_IF_ROW2& row = ifTable->Table[i];
            if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK && row.OperStatus == IfOperStatusUp) {
                totalOutBytes += static_cast<double>(row.OutOctets);
            }
        }
        FreeMibTable(ifTable);
    } else {
        std::cerr << "[MetricCollector] Failed to get Network interface table (Out)." << std::endl;
    }
    return totalOutBytes / (1024.0 * 1024.0); // Convert to MB
}

long long MetricCollector::collectUptime() {
    return static_cast<long long>(GetTickCount64() / 1000); // Convert to seconds
}

nlohmann::json MetricCollector::collectAll() {
    nlohmann::json j;
    j["cpu_usage"] = collectCpuUsage();
    j["ram_usage"] = collectRamUsage();
    j["disk_usage"] = collectDiskUsage();
    j["network_in"] = collectNetworkIn();
    j["network_out"] = collectNetworkOut();
    j["uptime"] = collectUptime();
    return j;
}
