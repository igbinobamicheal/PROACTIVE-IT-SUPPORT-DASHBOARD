-- Seed data for proactive_it_dashboard database
-- Seed default user (admin / admin123) is handled in schema.sql.
-- Here we can seed some initial devices and metric records for testing.

INSERT INTO devices (name, token, ip_address, status) VALUES 
('Main Web Server', 'e38a2e1d7f6a8e4c9b01d2c3f4e5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3', '192.168.10.101', 'online'),
('Primary DB Server', 'f49b3f2e8a7b9f5d0c12d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4', '192.168.10.102', 'online'),
('Backup Storage Host', 'a12b3c4d5e6f7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b', '192.168.10.200', 'offline')
ON CONFLICT (token) DO UPDATE SET name = EXCLUDED.name;

-- Seed initial metrics for 'Main Web Server' (device_id = 1)
INSERT INTO metrics (device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime) VALUES
(1, 12.5, 45.2, 58.1, 1024.5, 2048.2, 86400),
(1, 14.2, 45.5, 58.1, 1200.1, 2150.0, 86500),
(1, 15.8, 45.8, 58.2, 1450.4, 2500.6, 86600);

-- Seed initial metrics for 'Primary DB Server' (device_id = 2)
INSERT INTO metrics (device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime) VALUES
(2, 8.2, 82.1, 34.5, 512.0, 4096.0, 172800),
(2, 9.5, 82.3, 34.5, 600.0, 4200.0, 172900);
