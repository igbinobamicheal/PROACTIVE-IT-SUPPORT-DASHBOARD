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
        pqxx::work txn(*conn);
        
        pqxx::result res;
        if (!device.machineGuid.empty()) {
            res = txn.exec_prepared("create_device_v2", 
                                    device.name, 
                                    device.token, 
                                    device.ipAddress, 
                                    device.status, 
                                    device.machineGuid, 
                                    device.macAddress, 
                                    device.windowsVersion);
        } else {
            res = txn.exec_prepared("create_device", 
                                    device.name, 
                                    device.token, 
                                    device.ipAddress, 
                                    device.status);
        }
        
        if (!res.empty()) {
            device.id = res[0]["id"].as<int>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<Device> DeviceRepository::findAll() {
    std::vector<Device> devices;
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_all_devices");
        
        for (auto const &row : res) {
            Device d;
            d.id = row["id"].as<int>();
            d.name = row["name"].as<std::string>();
            d.token = row["token"].as<std::string>();
            d.ipAddress = row["ip_address"].as<std::string>();
            d.status = row["status"].as<std::string>();
            
            if (!row["machine_guid"].is_null()) {
                d.machineGuid = row["machine_guid"].as<std::string>();
            }
            if (!row["mac_address"].is_null()) {
                d.macAddress = row["mac_address"].as<std::string>();
            }
            if (!row["windows_version"].is_null()) {
                d.windowsVersion = row["windows_version"].as<std::string>();
            }
            if (!row["created_at"].is_null()) {
                d.createdAt = row["created_at"].as<std::string>();
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
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_device_by_id", id);
        if (!res.empty()) {
            Device d;
            d.id = res[0]["id"].as<int>();
            d.name = res[0]["name"].as<std::string>();
            d.token = res[0]["token"].as<std::string>();
            d.ipAddress = res[0]["ip_address"].as<std::string>();
            d.status = res[0]["status"].as<std::string>();
            
            if (!res[0]["machine_guid"].is_null()) {
                d.machineGuid = res[0]["machine_guid"].as<std::string>();
            }
            if (!res[0]["mac_address"].is_null()) {
                d.macAddress = res[0]["mac_address"].as<std::string>();
            }
            if (!res[0]["windows_version"].is_null()) {
                d.windowsVersion = res[0]["windows_version"].as<std::string>();
            }
            if (!res[0]["created_at"].is_null()) {
                d.createdAt = res[0]["created_at"].as<std::string>();
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
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_device_by_token", token);
        if (!res.empty()) {
            Device d;
            d.id = res[0]["id"].as<int>();
            d.name = res[0]["name"].as<std::string>();
            d.token = res[0]["token"].as<std::string>();
            d.ipAddress = res[0]["ip_address"].as<std::string>();
            d.status = res[0]["status"].as<std::string>();
            
            if (!res[0]["machine_guid"].is_null()) {
                d.machineGuid = res[0]["machine_guid"].as<std::string>();
            }
            if (!res[0]["mac_address"].is_null()) {
                d.macAddress = res[0]["mac_address"].as<std::string>();
            }
            if (!res[0]["windows_version"].is_null()) {
                d.windowsVersion = res[0]["windows_version"].as<std::string>();
            }
            if (!res[0]["created_at"].is_null()) {
                d.createdAt = res[0]["created_at"].as<std::string>();
            }
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findByToken: " << e.what() << std::endl;
    }
    return std::nullopt;
}

std::optional<Device> DeviceRepository::findByGuid(const std::string& machineGuid) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_device_by_guid", machineGuid);
        if (!res.empty()) {
            Device d;
            d.id = res[0]["id"].as<int>();
            d.name = res[0]["name"].as<std::string>();
            d.token = res[0]["token"].as<std::string>();
            d.ipAddress = res[0]["ip_address"].as<std::string>();
            d.status = res[0]["status"].as<std::string>();
            
            if (!res[0]["machine_guid"].is_null()) {
                d.machineGuid = res[0]["machine_guid"].as<std::string>();
            }
            if (!res[0]["mac_address"].is_null()) {
                d.macAddress = res[0]["mac_address"].as<std::string>();
            }
            if (!res[0]["windows_version"].is_null()) {
                d.windowsVersion = res[0]["windows_version"].as<std::string>();
            }
            if (!res[0]["created_at"].is_null()) {
                d.createdAt = res[0]["created_at"].as<std::string>();
            }
            return d;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in findByGuid: " << e.what() << std::endl;
    }
    return std::nullopt;
}

void DeviceRepository::update(const Device& device) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        txn.exec_prepared("update_device_details", 
                           device.name, 
                           device.ipAddress, 
                           device.macAddress, 
                           device.windowsVersion, 
                           device.token, 
                           device.status, 
                           device.id);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DeviceRepository] Error in update: " << e.what() << std::endl;
        throw;
    }
}
