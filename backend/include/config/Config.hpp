#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>

class Config {
public:
    std::string dbHost;
    int dbPort;
    std::string dbUser;
    std::string dbPassword;
    std::string dbSchema;
    
    std::string serverHost;
    int serverPort;
    
    std::string jwtSecret;
    int jwtExpirationHours;
    int bcryptWorkFactor;
    std::string registrationKey;

    /**
     * Loads the configuration from a JSON file.
     * @param filepath The path to the JSON configuration file.
     */
    void load(const std::string& filepath);

    /**
     * Singleton instance access.
     */
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

private:
    Config() = default;
};

#endif // CONFIG_HPP
