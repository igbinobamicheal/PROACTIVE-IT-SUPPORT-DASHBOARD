#include "repositories/UserRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_user", username);
        if (!res.empty()) {
            User user;
            user.id = res[0]["id"].as<int>();
            user.username = res[0]["username"].as<std::string>();
            user.passwordHash = res[0]["password_hash"].as<std::string>();
            if (!res[0]["created_at"].is_null()) {
                user.createdAt = res[0]["created_at"].as<std::string>();
            }
            return user;
        }
    } catch (const std::exception& e) {
        std::cerr << "[UserRepository] Error in findByUsername: " << e.what() << std::endl;
    }
    return std::nullopt;
}

void UserRepository::create(User& user) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        pqxx::result res = txn.exec_prepared("create_user", user.username, user.passwordHash);
        if (!res.empty()) {
            user.id = res[0]["id"].as<int>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[UserRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}
