#include "repositories/MetricRepository.hpp"
#include "database/Database.hpp"
#include <iostream>
#include <algorithm>

void MetricRepository::create(Metric& metric) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        
        // 1. Insert metric (returns generated ID via RETURNING id)
        pqxx::result res = txn.exec_prepared("create_metric", 
                                             metric.deviceId, 
                                             metric.cpuUsage, 
                                             metric.ramUsage, 
                                             metric.diskUsage, 
                                             metric.networkIn, 
                                             metric.networkOut, 
                                             metric.uptime);
        if (!res.empty()) {
            metric.id = res[0]["id"].as<int>();
        }
        
        // 2. Update device status to online
        txn.exec_prepared("update_device_status", "online", metric.deviceId);
        
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[MetricRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::optional<Metric> MetricRepository::findLatestForDevice(int deviceId) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_latest_metric", deviceId);
        
        if (!res.empty()) {
            Metric m;
            m.id = res[0]["id"].as<int>();
            m.deviceId = res[0]["device_id"].as<int>();
            m.cpuUsage = res[0]["cpu_usage"].as<double>();
            m.ramUsage = res[0]["ram_usage"].as<double>();
            m.diskUsage = res[0]["disk_usage"].as<double>();
            m.networkIn = res[0]["network_in"].as<double>();
            m.networkOut = res[0]["network_out"].as<double>();
            m.uptime = res[0]["uptime"].as<int64_t>();
            if (!res[0]["timestamp"].is_null()) {
                m.timestamp = res[0]["timestamp"].as<std::string>();
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
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_metric_history", deviceId, limit);
        
        for (auto const &row : res) {
            Metric m;
            m.id = row["id"].as<int>();
            m.deviceId = row["device_id"].as<int>();
            m.cpuUsage = row["cpu_usage"].as<double>();
            m.ramUsage = row["ram_usage"].as<double>();
            m.diskUsage = row["disk_usage"].as<double>();
            m.networkIn = row["network_in"].as<double>();
            m.networkOut = row["network_out"].as<double>();
            m.uptime = row["uptime"].as<int64_t>();
            if (!row["timestamp"].is_null()) {
                m.timestamp = row["timestamp"].as<std::string>();
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
