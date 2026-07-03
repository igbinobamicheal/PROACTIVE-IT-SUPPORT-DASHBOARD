-- Drop tables if they exist (ordered by foreign key dependencies)
DROP TABLE IF EXISTS metrics;
DROP TABLE IF EXISTS alerts;
DROP TABLE IF EXISTS device_diagnostics;
DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS registration_tokens;
DROP TABLE IF EXISTS device_users;
DROP TABLE IF EXISTS users;

-- 1. Users Table (for Authentication)
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 1b. Device Users Table (Employees assigned to devices)
CREATE TABLE device_users (
    id SERIAL PRIMARY KEY,
    full_name VARCHAR(255) NOT NULL,
    email VARCHAR(255) NOT NULL UNIQUE,
    department VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 1c. Registration Tokens Table
CREATE TABLE registration_tokens (
    id SERIAL PRIMARY KEY,
    token VARCHAR(255) NOT NULL UNIQUE,
    expires_at TIMESTAMP NOT NULL,
    used BOOLEAN DEFAULT FALSE,
    assigned_user_id INT REFERENCES device_users(id) ON DELETE SET NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 2. Devices Table (Monitoring Agents)
CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    token VARCHAR(255) NOT NULL UNIQUE,
    ip_address VARCHAR(45) NOT NULL,
    status VARCHAR(50) DEFAULT 'offline',
    assigned_user_id INT REFERENCES device_users(id) ON DELETE SET NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 3. Metrics Table (Ingested device metrics)
CREATE TABLE metrics (
    id SERIAL PRIMARY KEY,
    device_id INT NOT NULL,
    cpu_usage DOUBLE PRECISION NOT NULL,
    ram_usage DOUBLE PRECISION NOT NULL,
    disk_usage DOUBLE PRECISION NOT NULL,
    network_in DOUBLE PRECISION NOT NULL,
    network_out DOUBLE PRECISION NOT NULL,
    uptime BIGINT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 4. Alerts Table (Ingested thresholds alerts)
CREATE TABLE alerts (
    id SERIAL PRIMARY KEY,
    device_id INT NOT NULL,
    rule_type VARCHAR(50) NOT NULL,
    rule_value DOUBLE PRECISION NOT NULL,
    threshold DOUBLE PRECISION NOT NULL,
    message VARCHAR(255) NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    resolved BOOLEAN DEFAULT FALSE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- 5. Device Diagnostics Table (Troubleshooting profiles)
CREATE TABLE device_diagnostics (
    device_id INT PRIMARY KEY,
    system_info TEXT,
    cpu_info TEXT,
    memory_info TEXT,
    storage_info TEXT,
    battery_info TEXT,
    network_info TEXT,
    security_info TEXT,
    processes_info TEXT,
    event_logs TEXT,
    installed_software TEXT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
);

-- Seed default administrator account
INSERT INTO users (username, password_hash) 
VALUES ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02')
ON CONFLICT (username) DO NOTHING;
