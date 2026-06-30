#ifndef REGISTRATION_TOKEN_REPOSITORY_HPP
#define REGISTRATION_TOKEN_REPOSITORY_HPP

#include <string>
#include <optional>
#include <vector>

struct RegistrationToken {
    int id = 0;
    std::string token;
    bool used = false;
    bool isExpired = false;
    std::optional<int> assignedUserId;
};

class RegistrationTokenRepository {
public:
    void create(const std::string& token, const std::string& expiresAt, std::optional<int> assignedUserId = std::nullopt);
    std::optional<RegistrationToken> findByToken(const std::string& token);
    void useToken(const std::string& token);
    std::vector<RegistrationToken> findAll();
    void revokeToken(const std::string& token);
};

#endif // REGISTRATION_TOKEN_REPOSITORY_HPP
