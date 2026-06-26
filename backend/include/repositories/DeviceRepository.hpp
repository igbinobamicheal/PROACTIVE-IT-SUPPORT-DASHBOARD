#ifndef DEVICE_REPOSITORY_HPP
#define DEVICE_REPOSITORY_HPP

#include "models/Device.hpp"
#include <optional>
#include <vector>
#include <string>

class DeviceRepository {
public:
    /**
     * Creates a new device in the database.
     * @param device The Device object with details (name, ip_address, status). Generates and sets a unique token and id.
     */
    void create(Device& device);

    /**
     * Lists all registered devices in the database.
     * @return A list of Device objects.
     */
    std::vector<Device> findAll();

    /**
     * Finds a device by its integer ID.
     * @param id The device's ID.
     * @return An optional containing the Device if found, or std::nullopt.
     */
    std::optional<Device> findById(int id);

    /**
     * Finds a device by its unique authentication token.
     * @param token The device's unique token.
     * @return An optional containing the Device if found, or std::nullopt.
     */
    std::optional<Device> findByToken(const std::string& token);
};

#endif // DEVICE_REPOSITORY_HPP
