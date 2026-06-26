#include "database/Database.hpp"
#include "config/Config.hpp"
#include <stdexcept>
#include <iostream>

void Database::initialize() {
    auto& config = Config::getInstance();
    
    // Step 1: Connect to "sys" schema first to ensure "it_monitoring" exists
    std::string sysConnStr = "mysqlx://";
    sysConnStr += config.dbUser;
    if (!config.dbPassword.empty()) {
        sysConnStr += ":" + config.dbPassword;
    }
    sysConnStr += "@" + config.dbHost + ":" + std::to_string(config.dbPort) + "/sys";

    try {
        std::cout << "[Database] Verifying database schema exists..." << std::endl;
        mysqlx::Session sysSess(sysConnStr);
        sysSess.sql("CREATE DATABASE IF NOT EXISTS " + config.dbSchema).execute();
        sysSess.close();
    } catch (const mysqlx::Error& e) {
        std::cerr << "[Database] Warning: Failed to pre-create schema: " << e.what() << std::endl;
    }

    // Step 2: Now initialize X DevAPI client pool for target schema
    std::string connStr = "mysqlx://";
    connStr += config.dbUser;
    if (!config.dbPassword.empty()) {
        connStr += ":" + config.dbPassword;
    }
    connStr += "@" + config.dbHost + ":" + std::to_string(config.dbPort) + "/" + config.dbSchema;

    try {
        client = std::make_unique<mysqlx::Client>(connStr, mysqlx::ClientOption::POOL_MAX_SIZE, 10);
        
        // Step 3: Run table initializations
        mysqlx::Session sess = client->getSession();
        
        // 1. Users Table
        sess.sql("CREATE TABLE IF NOT EXISTS users ("
                 "  id INT AUTO_INCREMENT PRIMARY KEY,"
                 "  username VARCHAR(255) NOT NULL UNIQUE,"
                 "  password_hash VARCHAR(255) NOT NULL,"
                 "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;").execute();

        // 2. Devices Table
        sess.sql("CREATE TABLE IF NOT EXISTS devices ("
                 "  id INT AUTO_INCREMENT PRIMARY KEY,"
                 "  name VARCHAR(255) NOT NULL,"
                 "  token VARCHAR(255) NOT NULL UNIQUE,"
                 "  ip_address VARCHAR(45) NOT NULL,"
                 "  status VARCHAR(50) DEFAULT 'offline',"
                 "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;").execute();

        // 3. Metrics Table
        sess.sql("CREATE TABLE IF NOT EXISTS metrics ("
                 "  id INT AUTO_INCREMENT PRIMARY KEY,"
                 "  device_id INT NOT NULL,"
                 "  cpu_usage DOUBLE NOT NULL,"
                 "  ram_usage DOUBLE NOT NULL,"
                 "  disk_usage DOUBLE NOT NULL,"
                 "  network_in DOUBLE NOT NULL,"
                 "  network_out DOUBLE NOT NULL,"
                 "  uptime BIGINT NOT NULL,"
                 "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                 "  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;").execute();

        // 4. Alerts Table
        sess.sql("CREATE TABLE IF NOT EXISTS alerts ("
                 "  id INT AUTO_INCREMENT PRIMARY KEY,"
                 "  device_id INT NOT NULL,"
                 "  rule_type VARCHAR(50) NOT NULL,"
                 "  rule_value DOUBLE NOT NULL,"
                 "  threshold DOUBLE NOT NULL,"
                 "  message VARCHAR(255) NOT NULL,"
                 "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                 "  resolved BOOLEAN DEFAULT FALSE,"
                 "  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE"
                 ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;").execute();

        // 5. Seed default admin if missing (using INSERT IGNORE)
        // Password hash of "admin123" is: $2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02
        sess.sql("INSERT IGNORE INTO users (username, password_hash) VALUES ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02')").execute();

        sess.close();
        std::cout << "[Database] Successfully initialized connection pool and validated tables." << std::endl;
    } catch (const mysqlx::Error& e) {
        throw std::runtime_error("Failed to connect to MySQL via X DevAPI: " + std::string(e.what()));
    }
}

mysqlx::Session Database::getSession() {
    if (!client) {
        throw std::runtime_error("Database has not been initialized. Call initialize() first.");
    }
    return client->getSession();
}
