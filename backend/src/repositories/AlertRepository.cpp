#include "repositories/AlertRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

void AlertRepository::create(Alert& alert) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "INSERT INTO alerts (device_id, rule_type, rule_value, threshold, message) VALUES (?, ?, ?, ?, ?)"
        ));
        pstmt->setInt(1, alert.deviceId);
        pstmt->setString(2, alert.ruleType);
        pstmt->setDouble(3, alert.ruleValue);
        pstmt->setDouble(4, alert.threshold);
        pstmt->setString(5, alert.message);
        pstmt->executeUpdate();
        
        // Retrieve the generated AUTO_INCREMENT ID
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res->next()) {
            alert.id = res->getInt(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Alert> AlertRepository::findAll(int limit) {
    std::vector<Alert> alerts;
    try {
        auto conn = Database::getInstance().getConnection();
        std::string query = "SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, DATE_FORMAT(a.timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp, a.resolved "
                            "FROM alerts a "
                            "JOIN devices d ON a.device_id = d.id "
                            "ORDER BY a.timestamp DESC LIMIT " + std::to_string(limit);
        
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
        while (res->next()) {
            Alert a;
            a.id = res->getInt("id");
            a.deviceId = res->getInt("device_id");
            a.deviceName = res->getString("name");
            a.ruleType = res->getString("rule_type");
            a.ruleValue = res->getDouble("rule_value");
            a.threshold = res->getDouble("threshold");
            a.message = res->getString("message");
            a.timestamp = res->getString("timestamp");
            a.resolved = res->getBoolean("resolved");
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
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
            "SELECT a.id, a.device_id, d.name, a.rule_type, a.rule_value, a.threshold, a.message, DATE_FORMAT(a.timestamp, '%Y-%m-%d %H:%i:%s') AS timestamp, a.resolved "
            "FROM alerts a "
            "JOIN devices d ON a.device_id = d.id "
            "WHERE a.resolved = FALSE "
            "ORDER BY a.timestamp DESC"
        ));
                             
        while (res->next()) {
            Alert a;
            a.id = res->getInt("id");
            a.deviceId = res->getInt("device_id");
            a.deviceName = res->getString("name");
            a.ruleType = res->getString("rule_type");
            a.ruleValue = res->getDouble("rule_value");
            a.threshold = res->getDouble("threshold");
            a.message = res->getString("message");
            a.timestamp = res->getString("timestamp");
            a.resolved = res->getBoolean("resolved");
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
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id FROM alerts WHERE device_id = ? AND rule_type = ? AND resolved = FALSE LIMIT 1"
        ));
        pstmt->setInt(1, deviceId);
        pstmt->setString(2, ruleType);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        return res->next();
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in hasActiveAlert: " << e.what() << std::endl;
        return false;
    }
}

int AlertRepository::resolveAlert(int deviceId, const std::string& ruleType) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "UPDATE alerts SET resolved = TRUE WHERE device_id = ? AND rule_type = ? AND resolved = FALSE"
        ));
        pstmt->setInt(1, deviceId);
        pstmt->setString(2, ruleType);
        
        return pstmt->executeUpdate();
    } catch (const std::exception& e) {
        std::cerr << "[AlertRepository] Error in resolveAlert: " << e.what() << std::endl;
        return 0;
    }
}
