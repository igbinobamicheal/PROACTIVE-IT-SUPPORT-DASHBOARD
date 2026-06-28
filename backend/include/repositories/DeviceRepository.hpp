#ifndef DEVICE_REPOSITORY_HPP
#define DEVICE_REPOSITORY_HPP

#include "models/Device.hpp"
#include <optional>
#include <vector>
#include <string>

class DeviceRepository {
public:
    void create(Device& device);
    std::vector<Device> findAll();
    std::optional<Device> findById(int id);
    std::optional<Device> findByToken(const std::string& token);
    std::optional<Device> findByGuid(const std::string& machineGuid);
    void update(const Device& device);
};

#endif // DEVICE_REPOSITORY_HPP
