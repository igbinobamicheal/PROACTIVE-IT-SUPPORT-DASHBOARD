#pragma once

#include <string>

namespace Domain::Models {

struct MetricData {
    double cpu_usage = 0.0;
    double ram_usage = 0.0;
    double disk_usage = 0.0;
    long long uptime = 0;
    double network_in = 0.0;
    double network_out = 0.0;

    std::string hostname;
    std::string username;
    std::string windows_version;
    std::string mac_address;
    std::string local_ip;
    std::string machine_guid;
};

} // namespace Domain::Models
