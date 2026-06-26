#include "repositories/MetricRepository.hpp"
#include "database/Database.hpp"
#include <mysqlx/xdevapi.h>
#include <iostream>

void MetricRepository::create(Metric& metric) {
    try {
        auto session = Database::getInstance().getSession();
        
        // Start a transaction to ensure both operations succeed
        session.startTransaction();
        
        auto result = session.sql("INSERT INTO metrics (device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime) VALUES (?, ?, ?, ?, ?, ?, ?)")
                             .bind(metric.deviceId)
                             .bind(metric.cpuUsage)
                             .bind(metric.ramUsage)
                             .bind(metric.diskUsage)
                             .bind(metric.networkIn)
                             .bind(metric.networkOut)
                             .bind(metric.uptime)
                             .execute();
        
        metric.id = static_cast<int>(result.getAutoIncrementValue());
        
        // Update device status to online since it sent metrics
        session.sql("UPDATE devices SET status = 'online' WHERE id = ?")
               .bind(metric.deviceId)
               .execute();
               
        session.commit();
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::optional<Metric> MetricRepository::findLatestForDevice(int deviceId) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, DATE_FORMAT(timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp "
                                   "FROM metrics WHERE device_id = ? ORDER BY timestamp DESC LIMIT 1")
                             .bind(deviceId)
                             .execute();
        
        auto row = result.fetchOne();
        if (row) {
            Metric m;
            m.id = row[0].get<int>();
            m.deviceId = row[1].get<int>();
            m.cpuUsage = row[2].get<double>();
            m.ramUsage = row[3].get<double>();
            m.diskUsage = row[4].get<double>();
            m.networkIn = row[5].get<double>();
            m.networkOut = row[6].get<double>();
            m.uptime = row[7].get<int64_t>();
            if (!row[8].isNull()) {
                m.timestamp = row[8].get<std::string>();
            }
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
        auto session = Database::getInstance().getSession();
        std::string query = "SELECT id, device_id, cpu_usage, ram_usage, disk_usage, network_in, network_out, uptime, DATE_FORMAT(timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp "
                            "FROM metrics WHERE device_id = ? ORDER BY timestamp DESC LIMIT " + std::to_string(limit);
        
        auto result = session.sql(query)
                             .bind(deviceId)
                             .execute();
        
        while (auto row = result.fetchOne()) {
            Metric m;
            m.id = row[0].get<int>();
            m.deviceId = row[1].get<int>();
            m.cpuUsage = row[2].get<double>();
            m.ramUsage = row[3].get<double>();
            m.diskUsage = row[4].get<double>();
            m.networkIn = row[5].get<double>();
            m.networkOut = row[6].get<double>();
            m.uptime = row[7].get<int64_t>();
            if (!row[8].isNull()) {
                m.timestamp = row[8].get<std::string>();
            }
            history.push_back(m);
        }
        
        // Reverse history so it's in chronological order (oldest to newest) for chart plotting
        std::reverse(history.begin(), history.end());
        
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in findHistoryForDevice: " << e.what() << std::endl;
    }
    return history;
}

