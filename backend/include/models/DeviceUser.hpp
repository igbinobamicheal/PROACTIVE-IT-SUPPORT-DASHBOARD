#ifndef DEVICE_USER_HPP
#define DEVICE_USER_HPP

#include <string>
#include <nlohmann/json.hpp>

struct DeviceUser {
    int id = 0;
    std::string fullName;
    std::string email;
    std::string department;
};

inline void to_json(nlohmann::json& j, const DeviceUser& u) {
    j = nlohmann::json{
        {"id", u.id},
        {"full_name", u.fullName},
        {"email", u.email},
        {"department", u.department}
    };
}

inline void from_json(const nlohmann::json& j, DeviceUser& u) {
    if (j.contains("id")) j.at("id").get_to(u.id);
    j.at("full_name").get_to(u.fullName);
    j.at("email").get_to(u.email);
    if (j.contains("department") && !j["department"].is_null()) {
        j.at("department").get_to(u.department);
    } else {
        u.department = "";
    }
}

#endif // DEVICE_USER_HPP
