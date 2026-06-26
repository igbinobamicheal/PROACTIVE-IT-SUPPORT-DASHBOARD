#include "ConfigManager.h"
#include <fstream>
#include <iostream>

bool ConfigManager::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Failed to open: " << filepath << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;
        serverUrl = j.value("server_url", "http://localhost:8080");
        deviceName = j.value("device_name", "My_Agent_Device");
        deviceToken = j.value("device_token", "");
        intervalSeconds = j.value("interval_seconds", 10);
        adminUsername = j.value("admin_username", "");
        adminPassword = j.value("admin_password", "");
        registrationKey = j.value("registration_key", "");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] JSON Error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::save(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[ConfigManager] Failed to write: " << filepath << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        j["server_url"] = serverUrl;
        j["device_name"] = deviceName;
        j["device_token"] = deviceToken;
        j["interval_seconds"] = intervalSeconds;
        j["admin_username"] = adminUsername;
        j["admin_password"] = adminPassword;
        j["registration_key"] = registrationKey;
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] JSON Save Error: " << e.what() << std::endl;
        return false;
    }
}
