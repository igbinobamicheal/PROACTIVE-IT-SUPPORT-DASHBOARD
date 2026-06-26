#include "repositories/DeviceRepository.hpp"
#include "database/Database.hpp"
#include <mysqlx/xdevapi.h>
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
        auto session = Database::getInstance().getSession();
        auto result = session.sql("INSERT INTO devices (name, token, ip_address, status) VALUES (?, ?, ?, ?)")
                             .bind(device.name)
                             .bind(device.token)
                             .bind(device.ipAddress)
                             .bind(device.status)
                             .execute();
        
        device.id = static_cast<int>(result.getAutoIncrementValue());
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Device> DeviceRepository::findAll() {
    std::vector<Device> devices;
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices")
                             .execute();
        
        for (auto row : result.fetchAll()) {
            Device d;
            d.id = row[0].get<int>();
            d.name = row[1].get<std::string>();
            d.token = row[2].get<std::string>();
            d.ipAddress = row[3].get<std::string>();
            d.status = row[4].get<std::string>();
            if (!row[5].isNull()) {
                d.createdAt = row[5].get<std::string>();
            }
            devices.push_back(d);
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findAll: " << e.what() << std::endl;
    }
    return devices;
}

std::optional<Device> DeviceRepository::findById(int id) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices WHERE id = ?")
                             .bind(id)
                             .execute();
        
        auto row = result.fetchOne();
        if (row) {
            Device d;
            d.id = row[0].get<int>();
            d.name = row[1].get<std::string>();
            d.token = row[2].get<std::string>();
            d.ipAddress = row[3].get<std::string>();
            d.status = row[4].get<std::string>();
            if (!row[5].isNull()) {
                d.createdAt = row[5].get<std::string>();
            }
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findById: " << e.what() << std::endl;
    }
    return std::nullopt;
}

std::optional<Device> DeviceRepository::findByToken(const std::string& token) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id, name, token, ip_address, status, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM devices WHERE token = ?")
                             .bind(token)
                             .execute();
        
        auto row = result.fetchOne();
        if (row) {
            Device d;
            d.id = row[0].get<int>();
            d.name = row[1].get<std::string>();
            d.token = row[2].get<std::string>();
            d.ipAddress = row[3].get<std::string>();
            d.status = row[4].get<std::string>();
            if (!row[5].isNull()) {
                d.createdAt = row[5].get<std::string>();
            }
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findByToken: " << e.what() << std::endl;
    }
    return std::nullopt;
}
