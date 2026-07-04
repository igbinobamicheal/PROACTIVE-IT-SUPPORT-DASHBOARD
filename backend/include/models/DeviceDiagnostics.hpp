#ifndef DEVICE_DIAGNOSTICS_HPP
#define DEVICE_DIAGNOSTICS_HPP

#include <string>
#include <nlohmann/json.hpp>

struct DeviceDiagnostics {
    int deviceId = 0;
    std::string systemInfo = "{}";
    std::string cpuInfo = "{}";
    std::string memoryInfo = "{}";
    std::string storageInfo = "[]";
    std::string batteryInfo = "{}";
    std::string networkInfo = "{}";
    std::string securityInfo = "{}";
    std::string processesInfo = "{}";
    std::string eventLogs = "[]";
    std::string installedSoftware = "[]";
    std::string lastUpdated = "";
};

// Safe JSON parser to prevent crashes on invalid strings
inline nlohmann::json safeParseJson(const std::string& str, const std::string& fallback) {
    try {
        if (!str.empty() && str != "null") {
            return nlohmann::json::parse(str);
        }
    } catch (...) {}
    return nlohmann::json::parse(fallback);
}

// nlohmann/json integration for serialization/deserialization
inline void to_json(nlohmann::json& j, const DeviceDiagnostics& d) {
    auto sysJson = safeParseJson(d.systemInfo, "{}");
    nlohmann::json startupJson = nlohmann::json::array();
    if (sysJson.contains("startup_applications")) {
        startupJson = sysJson["startup_applications"];
        sysJson.erase("startup_applications");
    }
    j = nlohmann::json{
        {"device_id", d.deviceId},
        {"system_info", sysJson},
        {"cpu_info", safeParseJson(d.cpuInfo, "{}")},
        {"memory_info", safeParseJson(d.memoryInfo, "{}")},
        {"storage_info", safeParseJson(d.storageInfo, "[]")},
        {"battery_info", safeParseJson(d.batteryInfo, "{}")},
        {"network_info", safeParseJson(d.networkInfo, "{}")},
        {"security_info", safeParseJson(d.securityInfo, "{}")},
        {"processes_info", safeParseJson(d.processesInfo, "{}")},
        {"event_logs", safeParseJson(d.eventLogs, "[]")},
        {"installed_software", safeParseJson(d.installedSoftware, "[]")},
        {"startup_applications", startupJson},
        {"last_updated", d.lastUpdated}
    };
}

inline void from_json(const nlohmann::json& j, DeviceDiagnostics& d) {
    if (j.contains("device_id")) j.at("device_id").get_to(d.deviceId);
    
    if (j.contains("system_info")) {
        if (j["system_info"].is_string()) j.at("system_info").get_to(d.systemInfo);
        else d.systemInfo = j["system_info"].dump();
    }
    if (j.contains("cpu_info")) {
        if (j["cpu_info"].is_string()) j.at("cpu_info").get_to(d.cpuInfo);
        else d.cpuInfo = j["cpu_info"].dump();
    }
    if (j.contains("memory_info")) {
        if (j["memory_info"].is_string()) j.at("memory_info").get_to(d.memoryInfo);
        else d.memoryInfo = j["memory_info"].dump();
    }
    if (j.contains("storage_info")) {
        if (j["storage_info"].is_string()) j.at("storage_info").get_to(d.storageInfo);
        else d.storageInfo = j["storage_info"].dump();
    }
    if (j.contains("battery_info")) {
        if (j["battery_info"].is_string()) j.at("battery_info").get_to(d.batteryInfo);
        else d.batteryInfo = j["battery_info"].dump();
    }
    if (j.contains("network_info")) {
        if (j["network_info"].is_string()) j.at("network_info").get_to(d.networkInfo);
        else d.networkInfo = j["network_info"].dump();
    }
    if (j.contains("security_info")) {
        if (j["security_info"].is_string()) j.at("security_info").get_to(d.securityInfo);
        else d.securityInfo = j["security_info"].dump();
    }
    if (j.contains("processes_info")) {
        if (j["processes_info"].is_string()) j.at("processes_info").get_to(d.processesInfo);
        else d.processesInfo = j["processes_info"].dump();
    }
    if (j.contains("event_logs")) {
        if (j["event_logs"].is_string()) j.at("event_logs").get_to(d.eventLogs);
        else d.eventLogs = j["event_logs"].dump();
    }
    if (j.contains("installed_software")) {
        if (j["installed_software"].is_string()) j.at("installed_software").get_to(d.installedSoftware);
        else d.installedSoftware = j["installed_software"].dump();
    }
    if (j.contains("last_updated")) j.at("last_updated").get_to(d.lastUpdated);
}

#endif // DEVICE_DIAGNOSTICS_HPP
