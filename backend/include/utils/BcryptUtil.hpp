#ifndef BCRYPT_UTIL_HPP
#define BCRYPT_UTIL_HPP

#include <string>

class BcryptUtil {
public:
    /**
     * Generates a BCrypt hash for the given password.
     * @param password The plain text password.
     * @param workFactor The BCrypt work factor (cost). Defaults to 10.
     * @return The hashed password, or empty string on failure.
     */
    static std::string hashPassword(const std::string& password, int workFactor = 10);

    /**
     * Verifies a plain text password against a BCrypt hash.
     * @param password The plain text password.
     * @param hash The expected BCrypt hash.
     * @return true if password matches the hash, false otherwise.
     */
    static bool verifyPassword(const std::string& password, const std::string& hash);
};

#endif // BCRYPT_UTIL_HPP
