#include "repositories/UserRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

std::optional<User> UserRepository::findByUsername(const std::string& username) {
    try {
        auto conn = Database::getInstance().getConnection();
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT id, username, password_hash, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at FROM users WHERE username = ?"
        ));
        pstmt->setString(1, username);
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            User user;
            user.id = res->getInt("id");
            user.username = res->getString("username");
            user.passwordHash = res->getString("password_hash");
            user.createdAt = res->getString("created_at");
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
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "INSERT INTO users (username, password_hash) VALUES (?, ?)"
        ));
        pstmt->setString(1, user.username);
        pstmt->setString(2, user.passwordHash);
        pstmt->executeUpdate();
        
        // Retrieve the generated AUTO_INCREMENT ID
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
        if (res->next()) {
            user.id = res->getInt(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "[UserRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}
