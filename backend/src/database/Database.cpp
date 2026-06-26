#include "database/Database.hpp"
#include "config/Config.hpp"
#include <stdexcept>
#include <iostream>

void Database::initialize() {
    auto& config = Config::getInstance();
    
    dbUser = config.dbUser;
    dbPassword = config.dbPassword;
    dbSchema = config.dbSchema;
    
    // Classic MySQL connection URL format: tcp://host:port
    dbUrl = "tcp://" + config.dbHost + ":" + std::to_string(config.dbPort);
    
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        if (!driver) {
            throw std::runtime_error("Failed to get MySQL driver instance");
        }
        
        // Connect to sys/mysql default schema first to ensure our target database schema exists
        std::cout << "[Database] Verifying database schema exists..." << std::endl;
        std::unique_ptr<sql::Connection> tempConn(driver->connect(dbUrl, dbUser, dbPassword));
        std::unique_ptr<sql::Statement> stmt(tempConn->createStatement());
        stmt->execute("CREATE DATABASE IF NOT EXISTS " + dbSchema);
        
        // Select target schema on the temp connection to create tables
        tempConn->setSchema(dbSchema);
        
        // 1. Users Table
        stmt->execute("CREATE TABLE IF NOT EXISTS users ("
                      "  id INT AUTO_INCREMENT PRIMARY KEY,"
                      "  username VARCHAR(255) NOT NULL UNIQUE,"
                      "  password_hash VARCHAR(255) NOT NULL,"
                      "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;");

        // 2. Devices Table
        stmt->execute("CREATE TABLE IF NOT EXISTS devices ("
                      "  id INT AUTO_INCREMENT PRIMARY KEY,"
                      "  name VARCHAR(255) NOT NULL,"
                      "  token VARCHAR(255) NOT NULL UNIQUE,"
                      "  ip_address VARCHAR(45) NOT NULL,"
                      "  status VARCHAR(50) DEFAULT 'offline',"
                      "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;");

        // 3. Metrics Table
        stmt->execute("CREATE TABLE IF NOT EXISTS metrics ("
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
                      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;");

        // 4. Alerts Table
        stmt->execute("CREATE TABLE IF NOT EXISTS alerts ("
                      "  id INT AUTO_INCREMENT PRIMARY KEY,"
                      "  device_id INT NOT NULL,"
                      "  rule_type VARCHAR(50) NOT NULL,"
                      "  rule_value DOUBLE NOT NULL,"
                      "  threshold DOUBLE NOT NULL,"
                      "  message VARCHAR(255) NOT NULL,"
                      "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                      "  resolved BOOLEAN DEFAULT FALSE,"
                      "  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE"
                      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;");

        // 5. Seed default admin if missing (using INSERT IGNORE)
        stmt->execute("INSERT IGNORE INTO users (username, password_hash) VALUES ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02')");
        
        std::cout << "[Database] Successfully initialized classic connection pool and validated tables." << std::endl;
    } catch (const sql::SQLException& e) {
        throw std::runtime_error("Failed to initialize MySQL database via classic Connector: " + std::string(e.what()));
    }
}

Database::~Database() {
    std::lock_guard<std::mutex> lock(poolMutex);
    while (!connectionPool.empty()) {
        delete connectionPool.front();
        connectionPool.pop();
    }
}

void Database::ConnectionDeleter::operator()(sql::Connection* conn) const {
    Database::getInstance().returnConnection(conn);
}

void Database::returnConnection(sql::Connection* conn) {
    if (conn) {
        try {
            if (conn->isClosed()) {
                delete conn;
                std::lock_guard<std::mutex> lock(poolMutex);
                currentPoolSize--;
                return;
            }
        } catch (...) {
            delete conn;
            std::lock_guard<std::mutex> lock(poolMutex);
            currentPoolSize--;
            return;
        }
        
        std::lock_guard<std::mutex> lock(poolMutex);
        connectionPool.push(conn);
        poolCv.notify_one();
    }
}

Database::ConnectionPtr Database::getConnection() {
    std::unique_lock<std::mutex> lock(poolMutex);
    
    // If pool is empty, and we haven't reached maxPoolSize, create a new connection
    if (connectionPool.empty() && currentPoolSize < maxPoolSize) {
        try {
            sql::Connection* conn = driver->connect(dbUrl, dbUser, dbPassword);
            conn->setSchema(dbSchema);
            currentPoolSize++;
            return ConnectionPtr(conn);
        } catch (const sql::SQLException& e) {
            std::cerr << "[Database] Error creating connection: " << e.what() << std::endl;
            throw;
        }
    }
    
    // Otherwise wait for a connection to become available
    poolCv.wait(lock, [this]() { return !connectionPool.empty(); });
    
    sql::Connection* conn = connectionPool.front();
    connectionPool.pop();
    
    // Verify connection is alive, if not create a new one
    try {
        if (conn->isClosed()) {
            delete conn;
            conn = driver->connect(dbUrl, dbUser, dbPassword);
            conn->setSchema(dbSchema);
        }
    } catch (...) {
        delete conn;
        conn = driver->connect(dbUrl, dbUser, dbPassword);
        conn->setSchema(dbSchema);
    }
    
    return ConnectionPtr(conn);
}
