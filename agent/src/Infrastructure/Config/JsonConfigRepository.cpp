#include "Infrastructure/Config/JsonConfigRepository.h"
#include "Infrastructure/Logging/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace Infrastructure::Config {

JsonConfigRepository::JsonConfigRepository(const std::string& configFilePath)
    : configFilePath_(configFilePath) {}

bool JsonConfigRepository::Load(Domain::Models::AgentConfig& config) {
    using namespace Infrastructure::Logging;
    
    if (!std::filesystem::exists(configFilePath_)) {
        Logger::GetInstance().Warning("Config file not found at " + configFilePath_);
        return false;
    }

    try {
        std::ifstream file(configFilePath_);
        if (!file.is_open()) {
            Logger::GetInstance().Error("Failed to open config file for reading: " + configFilePath_);
            return false;
        }

        nlohmann::json j;
        file >> j;
        file.close();

        // Support both new and old config formats
        if (j.contains("server_url")) {
            config.server_url = j["server_url"].get<std::string>();
        } else if (j.contains("server")) {
            config.server_url = j["server"].get<std::string>();
        }

        if (j.contains("device_id")) {
            // Can be int or string representation of int
            if (j["device_id"].is_number()) {
                config.device_id = j["device_id"].get<int>();
            } else if (j["device_id"].is_string()) {
                config.device_id = std::stoi(j["device_id"].get<std::string>());
            }
        }

        if (j.contains("agent_secret")) {
            config.agent_secret = j["agent_secret"].get<std::string>();
        } else if (j.contains("device_token")) {
            config.agent_secret = j["device_token"].get<std::string>();
        } else if (j.contains("token")) {
            config.agent_secret = j["token"].get<std::string>();
        }

        Logger::GetInstance().Info("Configuration successfully loaded from " + configFilePath_);
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while loading config: " + std::string(e.what()));
        return false;
    }
}

bool JsonConfigRepository::Save(const Domain::Models::AgentConfig& config) {
    using namespace Infrastructure::Logging;

    try {
        std::filesystem::path p(configFilePath_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json j;
        j["server_url"] = config.server_url;
        j["device_id"] = config.device_id;
        j["agent_secret"] = config.agent_secret;

        std::ofstream file(configFilePath_);
        if (!file.is_open()) {
            Logger::GetInstance().Error("Failed to open config file for writing: " + configFilePath_);
            return false;
        }

        file << j.dump(4);
        file.close();

        Logger::GetInstance().Info("Configuration successfully saved to " + configFilePath_);
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while saving config: " + std::string(e.what()));
        return false;
    }
}

} // namespace Infrastructure::Config
