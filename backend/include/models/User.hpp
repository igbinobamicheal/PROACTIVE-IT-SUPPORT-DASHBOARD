#ifndef USER_HPP
#define USER_HPP

#include <string>
#include <nlohmann/json.hpp>

struct User {
    int id = 0;
    std::string username;
    std::string passwordHash;
    std::string createdAt;
};

// nlohmann/json integration for serialization/deserialization
inline void to_json(nlohmann::json& j, const User& u) {
    j = nlohmann::json{
        {"id", u.id},
        {"username", u.username},
        {"created_at", u.createdAt}
    };
}

inline void from_json(const nlohmann::json& j, User& u) {
    j.at("id").get_to(u.id);
    j.at("username").get_to(u.username);
    if (j.contains("password_hash")) {
        j.at("password_hash").get_to(u.passwordHash);
    }
    if (j.contains("created_at")) {
        j.at("created_at").get_to(u.createdAt);
    }
}

#endif // USER_HPP
