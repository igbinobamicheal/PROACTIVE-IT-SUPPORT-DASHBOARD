#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>

class ConfigManager {
public:
    std::string serverUrl;
    std::string deviceName;
    std::string deviceToken;
    int intervalSeconds = 10;
    std::string adminUsername;
    std::string adminPassword;
    std::string registrationKey;

    /**
     * Loads settings from local config.json.
     * @param filepath Path to local configuration.
     * @return true if successful, false otherwise.
     */
    bool load(const std::string& filepath);

    /**
     * Saves settings back to local config.json (useful for storing generated token).
     * @param filepath Path to local configuration.
     * @return true if successful, false otherwise.
     */
    bool save(const std::string& filepath);

    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

private:
    ConfigManager() = default;
};

#endif // CONFIG_MANAGER_H
