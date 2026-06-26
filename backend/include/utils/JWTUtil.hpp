#ifndef JWT_UTIL_HPP
#define JWT_UTIL_HPP

#include <string>

class JWTUtil {
public:
    /**
     * Generates a signed JWT token containing the username in the subject claim.
     * @param username The username to encode in the token.
     * @param secret The HMAC secret key.
     * @param expirationHours Token validity in hours.
     * @return The signed JWT string.
     */
    static std::string generateToken(const std::string& username, const std::string& secret, int expirationHours = 24);

    /**
     * Verifies the given JWT token and returns the username (subject) if valid.
     * @param token The JWT string to verify.
     * @param secret The HMAC secret key.
     * @return The username if the token is valid, or an empty string if verification fails.
     */
    static std::string verifyToken(const std::string& token, const std::string& secret);
};

#endif // JWT_UTIL_HPP
