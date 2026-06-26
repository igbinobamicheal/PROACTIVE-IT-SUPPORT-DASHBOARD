#include "config/Config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

void Config::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filepath);
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error("JSON parsing error in config file: " + std::string(e.what()));
    }

    // Set configuration values with defaults where appropriate
    dbHost = j.value("db_host", "localhost");
    dbPort = j.value("db_port", 33060);
    dbUser = j.value("db_user", "root");
    dbPassword = j.value("db_password", "");
    dbSchema = j.value("db_schema", "it_monitoring");

    serverHost = j.value("server_host", "0.0.0.0");
    serverPort = j.value("server_port", 8080);

    jwtSecret = j.value("jwt_secret", "default_secret_key_12345");
    jwtExpirationHours = j.value("jwt_expiration_hours", 24);
    bcryptWorkFactor = j.value("bcrypt_work_factor", 10);
    registrationKey = j.value("registration_key", "default_registration_key_12345");
}
