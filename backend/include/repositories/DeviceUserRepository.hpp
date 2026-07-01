#ifndef DEVICE_USER_REPOSITORY_HPP
#define DEVICE_USER_REPOSITORY_HPP

#include "models/DeviceUser.hpp"
#include <optional>
#include <vector>

class DeviceUserRepository {
public:
    void create(DeviceUser& user);
    std::vector<DeviceUser> findAll();
    std::optional<DeviceUser> findById(int id);
    std::optional<DeviceUser> findByEmail(const std::string& email);
    void update(const DeviceUser& user);
    void renameDepartment(const std::string& oldName, const std::string& newName);
};

#endif // DEVICE_USER_REPOSITORY_HPP
