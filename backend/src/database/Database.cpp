#include "database/Database.hpp"
#include "config/Config.hpp"
#include <stdexcept>
#include <iostream>

void Database::initialize() {
    auto& config = Config::getInstance();
    
    const char* databaseUrl = std::getenv("DATABASE_URL");
    if (databaseUrl && databaseUrl[0] != '\0') {
        dbUrlStr = databaseUrl;
        std::cout << "[Database] Using DATABASE_URL from environment." << std::endl;
    } else {
        bool isRemote = (config.dbHost != "localhost" && config.dbHost != "127.0.0.1" && config.dbHost != "postgres" && config.dbHost != "it_monitoring_db");
        
        // Connect to "postgres" database first to verify/create target database
        std::string sysConnStr = "host=" + config.dbHost + " port=" + std::to_string(config.dbPort) + " dbname=postgres user=" + config.dbUser + " password=" + config.dbPassword;
        if (isRemote) {
            sysConnStr += " sslmode=require";
        }

        try {
            std::cout << "[Database] Verifying database exists..." << std::endl;
            pqxx::connection sysConn(sysConnStr);
            pqxx::nontransaction txn(sysConn);
            pqxx::result res = txn.exec("SELECT 1 FROM pg_database WHERE datname = " + txn.quote(config.dbSchema));
            if (res.empty()) {
                std::cout << "[Database] Database '" << config.dbSchema << "' does not exist. Creating..." << std::endl;
                txn.exec("CREATE DATABASE " + txn.quote_name(config.dbSchema));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Database] Warning: Failed to pre-verify/create database: " << e.what() << std::endl;
        }

        // Build the connection string for target schema
        dbUrlStr = "host=" + config.dbHost + " port=" + std::to_string(config.dbPort) + " dbname=" + config.dbSchema + " user=" + config.dbUser + " password=" + config.dbPassword;
        if (isRemote) {
            dbUrlStr += " sslmode=require";
        }
    }

    try {
        std::cout << "[Database] Validating target database schema..." << std::endl;
        pqxx::connection tempConn(dbUrlStr);
        pqxx::work txn(tempConn);

        // 1. Users Table
        txn.exec("CREATE TABLE IF NOT EXISTS users ("
                 "  id SERIAL PRIMARY KEY,"
                 "  username VARCHAR(255) NOT NULL UNIQUE,"
                 "  password_hash VARCHAR(255) NOT NULL,"
                 "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                 ");");

        // 2. Devices Table
        txn.exec("CREATE TABLE IF NOT EXISTS devices ("
                 "  id SERIAL PRIMARY KEY,"
                 "  name VARCHAR(255) NOT NULL,"
                 "  token VARCHAR(255) NOT NULL UNIQUE,"
                 "  ip_address VARCHAR(45) NOT NULL,"
                 "  status VARCHAR(50) DEFAULT 'offline',"
                 "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                 ");");

        txn.exec("ALTER TABLE devices ADD COLUMN IF NOT EXISTS machine_guid VARCHAR(255) UNIQUE;");
        txn.exec("ALTER TABLE devices ADD COLUMN IF NOT EXISTS mac_address VARCHAR(50);");
        txn.exec("ALTER TABLE devices ADD COLUMN IF NOT EXISTS windows_version VARCHAR(255);");
        txn.exec("ALTER TABLE devices ADD COLUMN IF NOT EXISTS last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP;");

        // 2b. Registration Tokens Table
        txn.exec("CREATE TABLE IF NOT EXISTS registration_tokens ("
                 "  id SERIAL PRIMARY KEY,"
                 "  token VARCHAR(255) NOT NULL UNIQUE,"
                 "  expires_at TIMESTAMP NOT NULL,"
                 "  used BOOLEAN DEFAULT FALSE,"
                 "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                 ");");

        // 3. Metrics Table
        txn.exec("CREATE TABLE IF NOT EXISTS metrics ("
                 "  id SERIAL PRIMARY KEY,"
                 "  device_id INT NOT NULL,"
                 "  cpu_usage DOUBLE PRECISION NOT NULL,"
                 "  ram_usage DOUBLE PRECISION NOT NULL,"
                 "  disk_usage DOUBLE PRECISION NOT NULL,"
                 "  network_in DOUBLE PRECISION NOT NULL,"
                 "  network_out DOUBLE PRECISION NOT NULL,"
                 "  uptime BIGINT NOT NULL,"
                 "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                 "  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE"
                 ");");

        // 4. Alerts Table
        txn.exec("CREATE TABLE IF NOT EXISTS alerts ("
                 "  id SERIAL PRIMARY KEY,"
                 "  device_id INT NOT NULL,"
                 "  rule_type VARCHAR(50) NOT NULL,"
                 "  rule_value DOUBLE PRECISION NOT NULL,"
                 "  threshold DOUBLE PRECISION NOT NULL,"
                 "  message VARCHAR(255) NOT NULL,"
                 "  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                 "  resolved BOOLEAN DEFAULT FALSE,"
                 "  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE"
                 ");");

        // 5. Seed default admin if missing (using ON CONFLICT DO NOTHING)
        txn.exec("INSERT INTO users (username, password_hash) "
                 "VALUES ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02') "
                 "ON CONFLICT (username) DO NOTHING;");

        txn.commit();
        std::cout << "[Database] Successfully validated database tables." << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize PostgreSQL database: " + std::string(e.what()));
    }
}

Database::~Database() {
    std::lock_guard<std::mutex> lock(poolMutex);
    while (!connectionPool.empty()) {
        delete connectionPool.front();
        connectionPool.pop();
    }
}

void Database::ConnectionDeleter::operator()(pqxx::connection* conn) const {
    Database::getInstance().returnConnection(conn);
}

void Database::returnConnection(pqxx::connection* conn) {
    if (conn) {
        try {
            if (!conn->is_open()) {
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
    
    if (connectionPool.empty() && currentPoolSize < maxPoolSize) {
        try {
            pqxx::connection* conn = new pqxx::connection(dbUrlStr);
            prepareConnection(*conn);
            currentPoolSize++;
            return ConnectionPtr(conn);
        } catch (const std::exception& e) {
            std::cerr << "[Database] Error creating connection: " << e.what() << std::endl;
            throw;
        }
    }
    
    poolCv.wait(lock, [this]() { return !connectionPool.empty(); });
    
    pqxx::connection* conn = connectionPool.front();
    connectionPool.pop();
    
    try {
        if (!conn->is_open()) {
            delete conn;
            conn = new pqxx::connection(dbUrlStr);
            prepareConnection(*conn);
        }
    } catch (...) {
        delete conn;
        conn = new pqxx::connection(dbUrlStr);
        prepareConnection(*conn);
    }
    
    return ConnectionPtr(conn);
}

void Database::prepareConnection(pqxx::connection& conn) {
    // 1. Users queries
    conn.prepare("find_user", "SELECT id, username, password_hash, TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS') AS created_at FROM users WHERE username = $1");
    conn.prepare("create_user", "INSERT INTO users (username, password_hash) VALUES ($1, $2) RETURNING id");

    // 2. Devices queries
    conn.prepare("create_device", "INSERT INTO devices (name, token, ip_address, status) VALUES ($1, $2, $3, $4) RETURNING id");
    conn.prepare("create_device_v2", "INSERT INTO devices (name, token, ip_address, status, machine_guid, mac_address, windows_version) VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id");
    conn.prepare("find_all_devices", "SELECT id, name, token, ip_address, status, machine_guid, mac_address, windows_version, TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS') AS created_at FROM devices");
    conn.prepare("find_device_by_id", "SELECT id, name, token, ip_address, status, machine_guid, mac_address, windows_version, TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS') AS created_at FROM devices WHERE id = $1");
    conn.prepare("find_device_by_token", "SELECT id, name, token, ip_address, status, machine_guid, mac_address, windows_version, TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS') AS created_at FROM devices WHERE token = $1");
    conn.prepare("find_device_by_guid", "SELECT id, name, token, ip_address, status, machine_guid, mac_address, windows_version, TO_CHAR(created_at, 'YYYY-MM-DD HH24:MI:SS') AS created_at FROM devices WHERE machine_guid = $1");
    conn.prepare("update_device_details", "UPDATE devices SET name = $1, ip_address = $2, mac_address = $3, windows_version = $4, token = $5, status = $6 WHERE id = $7");

    // 2b. Registration Token queries
    conn.prepare("create_registration_token", "INSERT INTO registration_tokens (token, expires_at) VALUES ($1, $2) RETURNING id");
    conn.prepare("find_registration_token", "SELECT id, token, used, (expires_at < NOW()) AS is_expired FROM registration_tokens WHERE token = $1");
    conn.prepare("use_registration_token", "UPDATE registration_tokens SET used = TRUE WHERE token = $1");
    conn.prepare("find_all_registration_tokens", "SELECT id, token, used, (expires_at < NOW()) AS is_expired FROM registration_tokens ORDER BY created_at DESC");
    conn.prepare("revoke_registration_token", "UPDATE registration_tokens SET expires_at = NOW() WHERE token = $1");

    // 3. Metrics queries
    conn.prepare("create_metric", "INSERT INTO metrics (device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime) VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id");
    conn.prepare("update_device_status", "UPDATE devices SET status = $1, last_seen = NOW() WHERE id = $2");
    conn.prepare("find_latest_metric", "SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, TO_CHAR(timestamp, 'YYYY-MM-DD HH24:MI:SS') AS timestamp FROM metrics WHERE device_id = $1 ORDER BY timestamp DESC LIMIT 1");
    conn.prepare("find_metric_history", "SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, TO_CHAR(timestamp, 'YYYY-MM-DD HH24:MI:SS') AS timestamp FROM metrics WHERE device_id = $1 ORDER BY timestamp DESC LIMIT $2");

    // 4. Alerts queries
    conn.prepare("create_alert", "INSERT INTO alerts (device_id, rule_type, rule_value, threshold, message) VALUES ($1, $2, $3, $4, $5) RETURNING id");
    conn.prepare("find_all_alerts", "SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, TO_CHAR(a.timestamp, 'YYYY-MM-DD HH24:MI:SS') AS timestamp, a.resolved FROM alerts a JOIN devices d ON a.device_id = d.id ORDER BY a.timestamp DESC LIMIT $1");
    conn.prepare("find_active_alerts", "SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, TO_CHAR(a.timestamp, 'YYYY-MM-DD HH24:MI:SS') AS timestamp, a.resolved FROM alerts a JOIN devices d ON a.device_id = d.id WHERE a.resolved = FALSE ORDER BY a.timestamp DESC");
    conn.prepare("has_active_alert", "SELECT id FROM alerts WHERE device_id = $1 AND rule_type = $2 AND resolved = FALSE LIMIT 1");
    conn.prepare("resolve_alert", "UPDATE alerts SET resolved = TRUE WHERE device_id = $1 AND rule_type = $2 AND resolved = FALSE");
    
    // 5. StatusChecker queries
    conn.prepare("find_timed_out_devices", "SELECT id, name, ip_address FROM devices WHERE status = 'online' AND last_seen < NOW() - INTERVAL '30 seconds'");
}
