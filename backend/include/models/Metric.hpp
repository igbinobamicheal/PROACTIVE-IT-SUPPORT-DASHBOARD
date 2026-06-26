#ifndef METRIC_HPP
#define METRIC_HPP

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

struct Metric {
    int id = 0;
    int deviceId = 0;
    double cpuUsage = 0.0;
    double ramUsage = 0.0;
    double diskUsage = 0.0;
    double networkIn = 0.0;
    double networkOut = 0.0;
    int64_t uptime = 0;
    std::string timestamp;
};

// nlohmann/json integration for serialization/deserialization
inline void to_json(nlohmann::json& j, const Metric& m) {
    j = nlohmann::json{
        {"id", m.id},
        {"device_id", m.deviceId},
        {"cpu_usage", m.cpuUsage},
        {"ram_usage", m.ramUsage},
        {"disk_usage", m.diskUsage},
        {"network_in", m.networkIn},
        {"network_out", m.networkOut},
        {"uptime", m.uptime},
        {"timestamp", m.timestamp}
    };
}

inline void from_json(const nlohmann::json& j, Metric& m) {
    if (j.contains("id")) j.at("id").get_to(m.id);
    if (j.contains("device_id")) j.at("device_id").get_to(m.deviceId);
    j.at("cpu_usage").get_to(m.cpuUsage);
    j.at("ram_usage").get_to(m.ramUsage);
    j.at("disk_usage").get_to(m.diskUsage);
    j.at("network_in").get_to(m.networkIn);
    j.at("network_out").get_to(m.networkOut);
    j.at("uptime").get_to(m.uptime);
    if (j.contains("timestamp")) j.at("timestamp").get_to(m.timestamp);
}

#endif // METRIC_HPP
