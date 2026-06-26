#define JWT_DISABLE_PICOJSON
#include "utils/JWTUtil.hpp"
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <chrono>

using traits = jwt::traits::nlohmann_json;

std::string JWTUtil::generateToken(const std::string& username, const std::string& secret, int expirationHours) {
    try {
        auto token = jwt::create<traits>()
            .set_issuer("proactive_it_dashboard")
            .set_subject(username)
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(expirationHours))
            .sign(jwt::algorithm::hs256{secret});
        return token;
    } catch (...) {
        return "";
    }
}

std::string JWTUtil::verifyToken(const std::string& token, const std::string& secret) {
    try {
        auto decoded = jwt::decode<traits>(token);
        auto verifier = jwt::verify<traits>()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer("proactive_it_dashboard");
        
        verifier.verify(decoded);
        return decoded.get_subject();
    } catch (...) {
        // Verification failed (invalid signature, expired token, etc.)
        return "";
    }
}
