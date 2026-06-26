#include "repositories/AlertRepository.hpp"
#include "database/Database.hpp"
#include <mysqlx/xdevapi.h>
#include <iostream>

void AlertRepository::create(Alert& alert) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("INSERT INTO alerts (device_id, rule_type, rule_value, threshold, message) VALUES (?, ?, ?, ?, ?)")
                             .bind(alert.deviceId)
                             .bind(alert.ruleType)
                             .bind(alert.ruleValue)
                             .bind(alert.threshold)
                             .bind(alert.message)
                             .execute();
        
        alert.id = static_cast<int>(result.getAutoIncrementValue());
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Alert> AlertRepository::findAll(int limit) {
    std::vector<Alert> alerts;
    try {
        auto session = Database::getInstance().getSession();
        std::string query = "SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, DATE_FORMAT(a.timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp, a.resolved "
                            "FROM alerts a "
                            "JOIN devices d ON a.device_id = d.id "
                            "ORDER BY a.timestamp DESC LIMIT " + std::to_string(limit);
        
        auto result = session.sql(query).execute();
        while (auto row = result.fetchOne()) {
            Alert a;
            a.id = row[0].get<int>();
            a.deviceId = row[1].get<int>();
            a.deviceName = row[2].get<std::string>();
            a.ruleType = row[3].get<std::string>();
            a.ruleValue = row[4].get<double>();
            a.threshold = row[5].get<double>();
            a.message = row[6].get<std::string>();
            if (!row[7].isNull()) {
                a.timestamp = row[7].get<std::string>();
            }
            a.resolved = row[8].get<int>() != 0; // BOOLEAN matches tinyint/int in X DevAPI
            alerts.push_back(a);
        }
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in findAll: " << e.what() << std::endl;
    }
    return alerts;
}

std::vector<Alert> AlertRepository::findActive() {
    std::vector<Alert> alerts;
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, DATE_FORMAT(a.timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp, a.resolved "
                                   "FROM alerts a "
                                   "JOIN devices d ON a.device_id = d.id "
                                   "WHERE a.resolved = FALSE "
                                   "ORDER BY a.timestamp DESC")
                             .execute();
                             
        while (auto row = result.fetchOne()) {
            Alert a;
            a.id = row[0].get<int>();
            a.deviceId = row[1].get<int>();
            a.deviceName = row[2].get<std::string>();
            a.ruleType = row[3].get<std::string>();
            a.ruleValue = row[4].get<double>();
            a.threshold = row[5].get<double>();
            a.message = row[6].get<std::string>();
            if (!row[7].isNull()) {
                a.timestamp = row[7].get<std::string>();
            }
            a.resolved = row[8].get<int>() != 0;
            alerts.push_back(a);
        }
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in findActive: " << e.what() << std::endl;
    }
    return alerts;
}

bool AlertRepository::hasActiveAlert(int deviceId, const std::string& ruleType) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id FROM alerts WHERE device_id = ? AND rule_type = ? AND resolved = FALSE LIMIT 1")
                             .bind(deviceId)
                             .bind(ruleType)
                             .execute();
        
        auto row = result.fetchOne();
        return (bool)row;
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in hasActiveAlert: " << e.what() << std::endl;
        return false;
    }
}

int AlertRepository::resolveAlert(int deviceId, const std::string& ruleType) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("UPDATE alerts SET resolved = TRUE WHERE device_id = ? AND rule_type = ? AND resolved = FALSE")
                             .bind(deviceId)
                             .bind(ruleType)
                             .execute();
                             
        return static_cast<int>(result.getAffectedItemsCount());
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in resolveAlert: " << e.what() << std::endl;
        return 0;
    }
}
