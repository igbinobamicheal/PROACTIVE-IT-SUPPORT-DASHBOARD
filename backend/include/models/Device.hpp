#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <string>
#include <nlohmann/json.hpp>

struct Device {
    int id = 0;
    std::string name;
    std::string token;
    std::string ipAddress;
    std::string status = "offline";
    std::string machineGuid;
    std::string macAddress;
    std::string windowsVersion;
    std::string createdAt;
};

// nlohmann/json integration for serialization/deserialization
inline void to_json(nlohmann::json& j, const Device& d) {
    j = nlohmann::json{
        {"id", d.id},
        {"name", d.name},
        {"token", d.token},
        {"ip_address", d.ipAddress},
        {"status", d.status},
        {"machine_guid", d.machineGuid},
        {"mac_address", d.macAddress},
        {"windows_version", d.windowsVersion},
        {"created_at", d.createdAt}
    };
}

inline void from_json(const nlohmann::json& j, Device& d) {
    if (j.contains("id")) j.at("id").get_to(d.id);
    j.at("name").get_to(d.name);
    if (j.contains("token")) j.at("token").get_to(d.token);
    j.at("ip_address").get_to(d.ipAddress);
    if (j.contains("status")) j.at("status").get_to(d.status);
    if (j.contains("machine_guid")) j.at("machine_guid").get_to(d.machineGuid);
    if (j.contains("mac_address")) j.at("mac_address").get_to(d.macAddress);
    if (j.contains("windows_version")) j.at("windows_version").get_to(d.windowsVersion);
    if (j.contains("created_at")) j.at("created_at").get_to(d.createdAt);
}

#endif // DEVICE_HPP
