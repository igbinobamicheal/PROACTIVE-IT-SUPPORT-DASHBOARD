#include "repositories/RegistrationTokenRepository.hpp"
#include "database/Database.hpp"
#include <iostream>

void RegistrationTokenRepository::create(const std::string& token, const std::string& expiresAt, std::optional<int> assignedUserId) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        if (assignedUserId.has_value()) {
            txn.exec_prepared("create_registration_token", token, expiresAt, assignedUserId.value());
        } else {
            txn.exec_prepared("create_registration_token", token, expiresAt, nullptr);
        }
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[RegistrationTokenRepository] Error in create: " << e.what() << std::endl;
        throw;
    }
}

std::optional<RegistrationToken> RegistrationTokenRepository::findByToken(const std::string& token) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_registration_token", token);
        if (!res.empty()) {
            RegistrationToken rt;
            rt.id = res[0]["id"].as<int>();
            rt.token = res[0]["token"].as<std::string>();
            rt.used = res[0]["used"].as<bool>();
            rt.isExpired = res[0]["is_expired"].as<bool>();
            if (!res[0]["assigned_user_id"].is_null()) {
                rt.assignedUserId = res[0]["assigned_user_id"].as<int>();
            }
            return rt;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RegistrationTokenRepository] Error in findByToken: " << e.what() << std::endl;
    }
    return std::nullopt;
}

void RegistrationTokenRepository::useToken(const std::string& token) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        txn.exec_prepared("use_registration_token", token);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[RegistrationTokenRepository] Error in useToken: " << e.what() << std::endl;
        throw;
    }
}

std::vector<RegistrationToken> RegistrationTokenRepository::findAll() {
    std::vector<RegistrationToken> tokens;
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec_prepared("find_all_registration_tokens");
        for (auto const &row : res) {
            RegistrationToken rt;
            rt.id = row["id"].as<int>();
            rt.token = row["token"].as<std::string>();
            rt.used = row["used"].as<bool>();
            rt.isExpired = row["is_expired"].as<bool>();
            if (!row["assigned_user_id"].is_null()) {
                rt.assignedUserId = row["assigned_user_id"].as<int>();
            }
            tokens.push_back(rt);
        }
    } catch (const std::exception& e) {
        std::cerr << "[RegistrationTokenRepository] Error in findAll: " << e.what() << std::endl;
    }
    return tokens;
}

void RegistrationTokenRepository::revokeToken(const std::string& token) {
    try {
        auto conn = Database::getInstance().getConnection();
        pqxx::work txn(*conn);
        txn.exec_prepared("revoke_registration_token", token);
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[RegistrationTokenRepository] Error in revokeToken: " << e.what() << std::endl;
        throw;
    }
}
