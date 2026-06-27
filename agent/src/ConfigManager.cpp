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
        std::string rawUrl = j.value("server_url", j.value("server", "http://localhost:8080"));
        
        // Clean URL: prepend https:// if protocol is missing
        if (rawUrl.find("http://") != 0 && rawUrl.find("https://") != 0) {
            rawUrl = "https://" + rawUrl;
        }
        
        // Strip trailing slash if present
        if (!rawUrl.empty() && rawUrl.back() == '/') {
            rawUrl.pop_back();
        }
        
        // Strip trailing "/api" if present
        const std::string apiSuffix = "/api";
        if (rawUrl.size() >= apiSuffix.size() && 
            rawUrl.compare(rawUrl.size() - apiSuffix.size(), apiSuffix.size(), apiSuffix) == 0) {
            rawUrl = rawUrl.substr(0, rawUrl.size() - apiSuffix.size());
        }
        
        // Strip trailing slash again in case it was "/api/"
        if (!rawUrl.empty() && rawUrl.back() == '/') {
            rawUrl.pop_back();
        }
        
        serverUrl = rawUrl;
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
