#ifndef METRIC_COLLECTOR_H
#define METRIC_COLLECTOR_H

#include <nlohmann/json.hpp>

// Prevent windows.h from including winsock.h (Winsock 1.x)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>

class MetricCollector {
public:
    MetricCollector();
    
    /**
     * Collects current CPU usage percentage.
     */
    double collectCpuUsage();

    /**
     * Collects current RAM usage percentage.
     */
    double collectRamUsage();

    /**
     * Collects current Disk usage percentage.
     */
    double collectDiskUsage();

    /**
     * Collects network input Megabytes.
     */
    double collectNetworkIn();

    /**
     * Collects network output Megabytes.
     */
    double collectNetworkOut();

    /**
     * Collects host uptime in seconds.
     */
    long long collectUptime();

    /**
     * Aggregates all current metrics into a JSON object matching backend format.
     */
    nlohmann::json collectAll();

private:
    FILETIME prevIdleTime;
    FILETIME prevKernelTime;
    FILETIME prevUserTime;
    bool initialized;

    ULONGLONG subtractFileTimes(const FILETIME& ftA, const FILETIME& ftB);
};

#endif // METRIC_COLLECTOR_H
