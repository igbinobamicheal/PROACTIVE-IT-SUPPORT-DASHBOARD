#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "Domain/ServiceInterfaces/IMetricCollector.h"
#include <mutex>

namespace Infrastructure::Metrics {

class WindowsMetricCollector : public Domain::ServiceInterfaces::IMetricCollector {
public:
    WindowsMetricCollector();

    Domain::Models::MetricData Collect() override;

private:
    // Metric helpers
    double CollectCpuUsage();
    double CollectRamUsage();
    double CollectDiskUsage();
    long long CollectUptime();
    void CollectNetworkUsage(double& networkIn, double& networkOut);
    std::string CollectHostname();
    std::string CollectLoggedUsername();
    std::string CollectWindowsVersion();
    void CollectNetworkDetails(std::string& macAddress, std::string& localIp);
    std::string CollectMachineGuid();

    // Utility
    std::string WstringToUtf8(const std::wstring& wstr);
    ULONGLONG SubtractFileTimes(const FILETIME& ftA, const FILETIME& ftB);

    // CPU state
    bool cpuInitialized_;
    FILETIME prevIdleTime_;
    FILETIME prevKernelTime_;
    FILETIME prevUserTime_;
    std::mutex mutex_;
};

} // namespace Infrastructure::Metrics
