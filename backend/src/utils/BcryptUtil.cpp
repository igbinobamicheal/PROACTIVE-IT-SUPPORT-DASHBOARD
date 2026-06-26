#include "utils/BcryptUtil.hpp"
#include "../../third_party/libbcrypt/libbcrypt.h"
#include <stdexcept>
#include <cstring>

std::string BcryptUtil::hashPassword(const std::string& password, int workFactor) {
    char salt[BCRYPT_HASHSIZE] = {0};
    char hash[BCRYPT_HASHSIZE] = {0};

    if (bcrypt_gensalt(workFactor, salt) != 0) {
        throw std::runtime_error("Failed to generate salt for BCrypt");
    }

    if (bcrypt_hashpw(password.c_str(), salt, hash) != 0) {
        throw std::runtime_error("Failed to hash password with BCrypt");
    }

    return std::string(hash);
}

bool BcryptUtil::verifyPassword(const std::string& password, const std::string& hash) {
    if (hash.empty() || hash.length() >= BCRYPT_HASHSIZE) {
        return false;
    }
    
    char expected_hash[BCRYPT_HASHSIZE] = {0};
    std::strncpy(expected_hash, hash.c_str(), BCRYPT_HASHSIZE - 1);
    
    int result = bcrypt_checkpw(password.c_str(), expected_hash);
    return result == 0;
}


