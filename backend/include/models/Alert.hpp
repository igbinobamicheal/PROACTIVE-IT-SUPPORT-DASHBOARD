#ifndef ALERT_HPP
#define ALERT_HPP

#include <string>
#include <nlohmann/json.hpp>

struct Alert {
    int id = 0;
    int deviceId = 0;
    std::string deviceName; // Join helper field
    std::string ruleType;   // "cpu", "ram", "disk"
    double ruleValue = 0.0;
    double threshold = 0.0;
    std::string message;
    std::string timestamp;
    bool resolved = false;
};

// nlohmann/json integration for serialization/deserialization
inline void to_json(nlohmann::json& j, const Alert& a) {
    j = nlohmann::json{
        {"id", a.id},
        {"device_id", a.deviceId},
        {"device_name", a.deviceName},
        {"rule_type", a.ruleType},
        {"rule_value", a.ruleValue},
        {"threshold", a.threshold},
        {"message", a.message},
        {"timestamp", a.timestamp},
        {"resolved", a.resolved}
    };
}

inline void from_json(const nlohmann::json& j, Alert& a) {
    if (j.contains("id")) j.at("id").get_to(a.id);
    j.at("device_id").get_to(a.deviceId);
    if (j.contains("device_name")) j.at("device_name").get_to(a.deviceName);
    j.at("rule_type").get_to(a.ruleType);
    j.at("rule_value").get_to(a.ruleValue);
    j.at("threshold").get_to(a.threshold);
    j.at("message").get_to(a.message);
    if (j.contains("timestamp")) j.at("timestamp").get_to(a.timestamp);
    if (j.contains("resolved")) j.at("resolved").get_to(a.resolved);
}

#endif // ALERT_HPP
