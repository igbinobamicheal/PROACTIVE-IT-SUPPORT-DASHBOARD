#include "repositories/DeviceRepository.hpp"
#include "database/Database.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

static std::string generateDeviceToken() {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution;
    
    std::stringstream ss;
    // Generate 64 characters of random hex (256 bits of entropy)
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << distribution(generator);
    ss << std::setw(16) << distribution(generator);
    ss << std::setw(16) << distribution(generator);
    ss << std::setw(16) << distribution(generator);
    return ss.str();
}

void DeviceRepository::create(Device& device) {
    try {
        device.token = generateDeviceToken();
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "INSERT INTO devices (name, token, ip_address, status) VALUES (?, ?, ?, ?)"
        ));
        pstmt->setString(1, device.name);
        pstmt->setString(2, device.token);
        pstmt->setString(3, device.ipAddress);
        pstmt->setString(4, device.status);
        pstmt->executeUpdate();
        
        // Retrieve the generated AUTO_INCREMENT ID
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res->next()) {
            device.id = res->getInt(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Device> DeviceRepository::findAll() {
    std::vector<Device> devices;
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(
            "SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices"
        ));
        
        while (res->next()) {
            Device d;
            d.id = res->getInt("id");
            d.name = res->getString("name");
            d.token = res->getString("token");
            d.ipAddress = res->getString("ip_address");
            d.status = res->getString("status");
            d.createdAt = res->getString("created_at");
            devices.push_back(d);
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findAll: " << e.what() << std::endl;
    }
    return devices;
}

std::optional<Device> DeviceRepository::findById(int id) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices WHERE id = ?"
        ));
        pstmt->setInt(1, id);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            Device d;
            d.id = res->getInt("id");
            d.name = res->getString("name");
            d.token = res->getString("token");
            d.ipAddress = res->getString("ip_address");
            d.status = res->getString("status");
            d.createdAt = res->getString("created_at");
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findById: " << e.what() << std::endl;
    }
    return std::nullopt;
}

std::optional<Device> DeviceRepository::findByToken(const std::string& token) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices WHERE token = ?"
        ));
        pstmt->setString(1, token);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            Device d;
            d.id = res->getInt("id");
            d.name = res->getString("name");
            d.token = res->getString("token");
            d.ipAddress = res->getString("ip_address");
            d.status = res->getString("status");
            d.createdAt = res->getString("created_at");
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findByToken: " << e.what() << std::endl;
    }
    return std::nullopt;
}
