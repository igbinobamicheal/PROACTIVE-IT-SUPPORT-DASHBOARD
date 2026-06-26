#include "repositories/UserRepository.hpp"
#include "database/Database.hpp"
#include <mysqlx/xdevapi.h>
#include <iostream>

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    try {
        auto session = Database::getInstance().getSession();
        auto result = session.sql("SELECT id, username, password_hash, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM users WHERE username = ?")
                             .bind(username)
                             .execute();
        
        auto row = result.fetchOne();
        if (row) {
            User user;
            user.id = row[0].get<int>();
            user.username = row[1].get<std::string>();
            user.passwordHash = row[2].get<std::string>();
            if (!row[3].isNull()) {
                user.createdAt = row[3].get<std::string>();
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
        auto session = Database::getInstance().getSession();
        auto result = session.sql("INSERT INTO users (username, password_hash) VALUES (?, ?)")
                             .bind(user.username)
                             .bind(user.passwordHash)
                             .execute();
        
        user.id = static_cast<int>(result.getAutoIncrementValue());
    } catch (const std::exception& e) {
        std::cerr << "[UserRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}
