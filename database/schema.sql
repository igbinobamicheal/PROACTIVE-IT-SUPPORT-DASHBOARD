-- Create database if not exists
CREATE DATABASE IF NOT EXISTS it_monitoring;
USE it_monitoring;

-- Drop tables if they exist (ordered by foreign key dependencies)
DROP TABLE IF EXISTS metrics;
DROP TABLE IF EXISTS alerts;
DROP TABLE IF EXISTS devices;
DROP TABLE IF EXISTS users;

-- 1. Users Table (for Authentication)
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2. Devices Table (Monitoring Agents)
CREATE TABLE devices (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    token VARCHAR(255) NOT NULL UNIQUE,
    ip_address VARCHAR(45) NOT NULL,
    status VARCHAR(50) DEFAULT 'offline',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3. Metrics Table (Ingested device metrics)
CREATE TABLE metrics (
    id INT AUTO_INCREMENT PRIMARY KEY,
    device_id INT NOT NULL,
    cpu_usage DOUBLE NOT NULL,
    ram_usage DOUBLE NOT NULL,
    disk_usage DOUBLE NOT NULL,
    network_in DOUBLE NOT NULL,
    network_out DOUBLE NOT NULL,
    uptime BIGINT NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4. Alerts Table (Ingested thresholds alerts)
CREATE TABLE alerts (
    id INT AUTO_INCREMENT PRIMARY KEY,
    device_id INT NOT NULL,
    rule_type VARCHAR(50) NOT NULL,
    rule_value DOUBLE NOT NULL,
    threshold DOUBLE NOT NULL,
    message VARCHAR(255) NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    resolved BOOLEAN DEFAULT FALSE,
    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Seed default administrator account
-- Password: admin123
-- BCrypt hash (cost 10) of "admin123" is: $2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02
INSERT INTO users (username, password_hash) 
VALUES ('admin', '$2a$10$N9qo8uLOickgx2ZMRZoMyeMiMwiT/hYv/GXAzJLiUmBNwmYD4kh02')
ON DUPLICATE KEY UPDATE username = username;

