#include "repositories/DeviceUserRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

void DeviceUserRepository::create(DeviceUser& user) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_prepared("create_device_user", user.fullName, user.email, user.department);
        if (!res.empty()) {
            user.id = res[0]["id"].as<int>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DeviceUserRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::vector<DeviceUser> DeviceUserRepository::findAll() {
    std::vector<DeviceUser> users;
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_all_device_users");
        for (auto const &row : res) {
            DeviceUser u;
            u.id = row["id"].as<int>();
            u.fullName = row["full_name"].as<std::string>();
            u.email = row["email"].as<std::string>();
            if (!row["department"].is_null()) {
                u.department = row["department"].as<std::string>();
            } else {
                u.department = "";
            }
            users.push_back(u);
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceUserRepository] Error in findAll: " << e.what() << std::endl;
    }
    return users;
}

std::optional<DeviceUser> DeviceUserRepository::findById(int id) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_device_user_by_id", id);
        if (!res.empty()) {
            DeviceUser u;
            u.id = res[0]["id"].as<int>();
            u.fullName = res[0]["full_name"].as<std::string>();
            u.email = res[0]["email"].as<std::string>();
            if (!res[0]["department"].is_null()) {
                u.department = res[0]["department"].as<std::string>();
            } else {
                u.department = "";
            }
            return u;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceUserRepository] Error in findById: " << e.what() << std::endl;
    }
    return std::nullopt;
}

std::optional<DeviceUser> DeviceUserRepository::findByEmail(const std::string& email) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_device_user_by_email", email);
        if (!res.empty()) {
            DeviceUser u;
            u.id = res[0]["id"].as<int>();
            u.fullName = res[0]["full_name"].as<std::string>();
            u.email = res[0]["email"].as<std::string>();
            if (!res[0]["department"].is_null()) {
                u.department = res[0]["department"].as<std::string>();
            } else {
                u.department = "";
            }
            return u;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeviceUserRepository] Error in findByEmail: " << e.what() << std::endl;
    }
    return std::nullopt;
}

void DeviceUserRepository::update(const DeviceUser& user) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        txn.exec_prepared("update_device_user", user.fullName, user.department, user.id);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DeviceUserRepository] Error in update: " << e.what() << std::endl;
        throw;
    }
}
