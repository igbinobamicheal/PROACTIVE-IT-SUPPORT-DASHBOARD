#include "repositories/MetricRepository.hpp"
#include "database/Database.hpp"
#include <iostream>
#include <algorithm>

void MetricRepository::create(Metric& metric) {
    auto conn = Database::getInstance().getConnection();
    try {
        // Start a transaction to ensure both operations succeed
        conn->setAutoCommit(false);
        
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "INSERT INTO metrics (device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime) VALUES (?, ?, ?, ?, ?, ?, ?)"
        ));
        pstmt->setInt(1, metric.deviceId);
        pstmt->setDouble(2, metric.cpuUsage);
        pstmt->setDouble(3, metric.ramUsage);
        pstmt->setDouble(4, metric.diskUsage);
        pstmt->setDouble(5, metric.networkIn);
        pstmt->setDouble(6, metric.networkOut);
        pstmt->setInt64(7, metric.uptime);
        pstmt->executeUpdate();
        
        // Retrieve the generated AUTO_INCREMENT ID
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res->next()) {
            metric.id = res->getInt(1);
        }
        
        // Update device status to online since it sent metrics
        std::unique_ptr<sql::PreparedStatement> pstmtUpdate(conn->prepareStatement(
            "UPDATE devices SET status = 'online' WHERE id = ?"
        ));
        pstmtUpdate->setInt(1, metric.deviceId);
        pstmtUpdate->executeUpdate();
               
        conn->commit();
        conn->setAutoCommit(true);
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in create: " << e.what() << std::endl;
        try {
            conn->rollback();
            conn->setAutoCommit(true);
        } catch (...) {}
        throw;
    }
}

std::optional<Metric> MetricRepository::findLatestForDevice(int deviceId) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, DATE_FORMAT(timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp "
            "FROM metrics WHERE device_id = ? ORDER BY timestamp DESC LIMIT 1"
        ));
        pstmt->setInt(1, deviceId);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            Metric m;
            m.id = res->getInt("id");
            m.deviceId = res->getInt("device_id");
            m.cpuUsage = res->getDouble("cpu_usage");
            m.ramUsage = res->getDouble("ram_usage");
            m.diskUsage = res->getDouble("disk_usage");
            m.networkIn = res->getDouble("network_in");
            m.networkOut = res->getDouble("network_out");
            m.uptime = res->getInt64("uptime");
            m.timestamp = res->getString("timestamp");
            return m;
        }
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in findLatestForDevice: " << e.what() << std::endl;
    }
    return std::nullopt;
}

std::vector<Metric> MetricRepository::findHistoryForDevice(int deviceId, int limit) {
    std::vector<Metric> history;
    try {
        auto conn = Database::getInstance().getConnection();
        // Since limit needs to be part of query or bound (limit is safer formatted here)
        std::string query = "SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, DATE_FORMAT(timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp "
                            "FROM metrics WHERE device_id = ? ORDER BY timestamp DESC LIMIT " + std::to_string(limit);
        
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
        pstmt->setInt(1, deviceId);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res->next()) {
            Metric m;
            m.id = res->getInt("id");
            m.deviceId = res->getInt("device_id");
            m.cpuUsage = res->getDouble("cpu_usage");
            m.ramUsage = res->getDouble("ram_usage");
            m.diskUsage = res->getDouble("disk_usage");
            m.networkIn = res->getDouble("network_in");
            m.networkOut = res->getDouble("network_out");
            m.uptime = res->getInt64("uptime");
            m.timestamp = res->getString("timestamp");
            history.push_back(m);
        }
        
        // Reverse history so it's in chronological order (oldest to newest) for chart plotting
        std::reverse(history.begin(), history.end());
        
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in findHistoryForDevice: " << e.what() << std::endl;
    }
    return history;
}
