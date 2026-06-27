#include "repositories/AlertRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

void AlertRepository::create(Alert& alert) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        
        pqxx::result res = txn.exec_prepared("create_alert", 
                                             alert.deviceId, 
                                             alert.ruleType, 
                                             alert.ruleValue, 
                                             alert.threshold, 
                                             alert.message);
        if (!res.empty()) {
            alert.id = res[0]["id"].as<int>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Alert> AlertRepository::findAll(int limit) {
    std::vector<Alert> alerts;
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_all_alerts", limit);
        
        for (auto const &row : res) {
            Alert a;
            a.id = row["id"].as<int>();
            a.deviceId = row["device_id"].as<int>();
            a.deviceName = row["name"].as<std::string>();
            a.ruleType = row["rule_type"].as<std::string>();
            a.ruleValue = row["rule_value"].as<double>();
            a.threshold = row["threshold"].as<double>();
            a.message = row["message"].as<std::string>();
            if (!row["timestamp"].is_null()) {
                a.timestamp = row["timestamp"].as<std::string>();
            }
            a.resolved = row["resolved"].as<bool>();
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
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_active_alerts");
        
        for (auto const &row : res) {
            Alert a;
            a.id = row["id"].as<int>();
            a.deviceId = row["device_id"].as<int>();
            a.deviceName = row["name"].as<std::string>();
            a.ruleType = row["rule_type"].as<std::string>();
            a.ruleValue = row["rule_value"].as<double>();
            a.threshold = row["threshold"].as<double>();
            a.message = row["message"].as<std::string>();
            if (!row["timestamp"].is_null()) {
                a.timestamp = row["timestamp"].as<std::string>();
            }
            a.resolved = row["resolved"].as<bool>();
            alerts.push_back(a);
        }
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in findActive: " << e.what() << std::endl;
    }
    return alerts;
}

bool AlertRepository::hasActiveAlert(int deviceId, const std::string& ruleType) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("has_active_alert", deviceId, ruleType);
        return !res.empty();
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in hasActiveAlert: " << e.what() << std::endl;
        return false;
    }
}

int AlertRepository::resolveAlert(int deviceId, const std::string& ruleType) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_prepared("resolve_alert", deviceId, ruleType);
        int rows = static_cast<int>(res.affected_rows());
        txn.commit();
        return rows;
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in resolveAlert: " << e.what() << std::endl;
        return 0;
    }
}
