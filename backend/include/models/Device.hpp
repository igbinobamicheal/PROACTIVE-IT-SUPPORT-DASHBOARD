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
        {"created_at", d.createdAt}
    };
}

inline void from_json(const nlohmann::json& j, Device& d) {
    if (j.contains("id")) j.at("id").get_to(d.id);
    j.at("name").get_to(d.name);
    if (j.contains("token")) j.at("token").get_to(d.token);
    j.at("ip_address").get_to(d.ipAddress);
    if (j.contains("status")) j.at("status").get_to(d.status);
    if (j.contains("created_at")) j.at("created_at").get_to(d.createdAt);
}

#endif // DEVICE_HPP
