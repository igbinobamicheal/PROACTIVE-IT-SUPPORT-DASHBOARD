#ifndef DEVICE_REGISTRAR_H
#define DEVICE_REGISTRAR_H

#include <string>

class DeviceRegistrar {
public:
    DeviceRegistrar() = default;

    /**
     * Attempts to log in using administrator credentials to obtain a user JWT token.
     * @param serverUrl URL of the dashboard server.
     * @param username Administrator username.
     * @param password Administrator password.
     * @return The JWT token if successful, or an empty string on failure.
     */
    std::string loginAndGetToken(const std::string& serverUrl, const std::string& username, const std::string& password);

    /**
     * Attempts to register the agent with the server and retrieve a device token.
     * Requires a valid administrator JWT token for authorization.
     * @param serverUrl URL of the dashboard server.
     * @param deviceName Name for this device.
     * @param adminJwt JWT token of the administrator.
     * @param tokenOut String to receive the generated device token.
     * @return true if successful, false otherwise.
     */
    bool registerDevice(const std::string& serverUrl, const std::string& deviceName, const std::string& adminJwt, std::string& tokenOut);
    bool registerDeviceWithKey(const std::string& serverUrl, const std::string& deviceName, const std::string& registrationKey, std::string& tokenOut);
};

#endif // DEVICE_REGISTRAR_H
