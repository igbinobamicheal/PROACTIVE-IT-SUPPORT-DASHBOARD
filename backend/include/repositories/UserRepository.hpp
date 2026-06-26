#ifndef USER_REPOSITORY_HPP
#define USER_REPOSITORY_HPP

#include "models/User.hpp"
#include <optional>
#include <string>

class UserRepository {
public:
    /**
     * Finds a user by their username.
     * @param username The username to search for.
     * @return An optional containing the User if found, or std::nullopt.
     */
    std::optional<User> findByUsername(const std::string& username);

    /**
     * Creates a new user in the database.
     * @param user The User object containing username and password hash.
     */
    void create(User& user);
};

#endif // USER_REPOSITORY_HPP
