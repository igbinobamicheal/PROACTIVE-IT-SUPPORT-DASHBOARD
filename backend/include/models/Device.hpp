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
    
    // Assigned user fields
    std::optional<int> assignedUserId;
    std::optional<std::string> assignedUserFullName;
    std::optional<std::string> assignedUserEmail;
    std::optional<std::string> assignedUserDepartment;
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
    if (d.assignedUserId.has_value()) {
        j["assigned_user_id"] = d.assignedUserId.value();
    } else {
        j["assigned_user_id"] = nullptr;
    }
    if (d.assignedUserFullName.has_value()) {
        j["assigned_user_name"] = d.assignedUserFullName.value();
    } else {
        j["assigned_user_name"] = nullptr;
    }
    if (d.assignedUserEmail.has_value()) {
        j["assigned_user_email"] = d.assignedUserEmail.value();
    } else {
        j["assigned_user_email"] = nullptr;
    }
    if (d.assignedUserDepartment.has_value()) {
        j["assigned_user_dept"] = d.assignedUserDepartment.value();
    } else {
        j["assigned_user_dept"] = nullptr;
    }
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
    
    if (j.contains("assigned_user_id") && !j["assigned_user_id"].is_null()) {
        d.assignedUserId = j["assigned_user_id"].get<int>();
    } else {
        d.assignedUserId = std::nullopt;
    }
    if (j.contains("assigned_user_name") && !j["assigned_user_name"].is_null()) {
        d.assignedUserFullName = j["assigned_user_name"].get<std::string>();
    } else {
        d.assignedUserFullName = std::nullopt;
    }
    if (j.contains("assigned_user_email") && !j["assigned_user_email"].is_null()) {
        d.assignedUserEmail = j["assigned_user_email"].get<std::string>();
    } else {
        d.assignedUserEmail = std::nullopt;
    }
    if (j.contains("assigned_user_dept") && !j["assigned_user_dept"].is_null()) {
        d.assignedUserDepartment = j["assigned_user_dept"].get<std::string>();
    } else {
        d.assignedUserDepartment = std::nullopt;
    }
}

#endif // DEVICE_HPP
