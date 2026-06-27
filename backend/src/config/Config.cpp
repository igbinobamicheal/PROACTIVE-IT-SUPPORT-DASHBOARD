#include "config/Config.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::string getEnvString(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value && value[0] != '\0' ? std::string(value) : fallback;
}

int getEnvInt(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }

    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        std::cerr << "[Config] Invalid integer for " << name << ": " << value
                  << ". Using " << fallback << "." << std::endl;
        return fallback;
    }
}
}

void Config::load(const std::string& filepath) {
    nlohmann::json j = nlohmann::json::object();
    std::ifstream file(filepath);
    if (file.is_open()) {
        try {
            file >> j;
        } catch (const std::exception& e) {
            throw std::runtime_error("JSON parsing error in config file: " + std::string(e.what()));
        }
    } else {
        std::cout << "[Config] Config file not found at " << filepath
                  << ". Using defaults and environment variables." << std::endl;
    }

    // Set configuration values with defaults where appropriate
    dbHost = j.value("db_host", "localhost");
    dbPort = j.value("db_port", 5432);
    dbUser = j.value("db_user", "root");
    dbPassword = j.value("db_password", "");
    dbSchema = j.value("db_schema", "it_monitoring");

    serverHost = j.value("server_host", "0.0.0.0");
    serverPort = j.value("server_port", 8080);

    jwtSecret = j.value("jwt_secret", "default_secret_key_12345");
    jwtExpirationHours = j.value("jwt_expiration_hours", 24);
    bcryptWorkFactor = j.value("bcrypt_work_factor", 10);
    registrationKey = j.value("registration_key", "default_registration_key_12345");

    // Environment variables override config.json values for cloud deployments.
    dbHost = getEnvString("DB_HOST", dbHost);
    dbPort = getEnvInt("DB_PORT", dbPort);
    dbUser = getEnvString("DB_USER", dbUser);
    dbPassword = getEnvString("DB_PASSWORD", dbPassword);
    dbSchema = getEnvString("DB_NAME", dbSchema);

    serverHost = getEnvString("SERVER_HOST", serverHost);
    serverPort = getEnvInt("SERVER_PORT", serverPort);
    serverPort = getEnvInt("PORT", serverPort);

    jwtSecret = getEnvString("JWT_SECRET", jwtSecret);
    jwtExpirationHours = getEnvInt("JWT_EXPIRATION_HOURS", jwtExpirationHours);
    bcryptWorkFactor = getEnvInt("BCRYPT_WORK_FACTOR", bcryptWorkFactor);
    registrationKey = getEnvString("REGISTRATION_KEY", registrationKey);
}
