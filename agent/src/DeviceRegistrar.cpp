#include "DeviceRegistrar.h"
#include "HttpClient.h"
#include <nlohmann/json.hpp>
#include <iostream>

std::string DeviceRegistrar::loginAndGetToken(const std::string& serverUrl, const std::string& username, const std::string& password) {
    HttpClient client;
    nlohmann::json payload;
    payload["username"] = username;
    payload["password"] = password;

    std::string response;
    std::string url = serverUrl + "/api/login";
    
    std::cout << "[DeviceRegistrar] Attempting login to retrieve JWT..." << std::endl;
    int status = client.post(url, payload.dump(), "", response);
    
    if (status == 200) {
        try {
            auto resJson = nlohmann::json::parse(response);
            if (resJson.contains("token")) {
                std::cout << "[DeviceRegistrar] Login successful." << std::endl;
                return resJson["token"].get<std::string>();
            }
        } catch (const std::exception& e) {
            std::cerr << "[DeviceRegistrar] Failed to parse login response: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[DeviceRegistrar] Login failed. HTTP Status: " << status << ", Response: " << response << std::endl;
    }
    return "";
}

bool DeviceRegistrar::registerDevice(const std::string& serverUrl, const std::string& deviceName, const std::string& adminJwt, std::string& tokenOut) {
    HttpClient client;
    nlohmann::json payload;
    payload["name"] = deviceName;
    payload["ip_address"] = "127.0.0.1"; // Default local loopback/local registration

    std::string response;
    std::string url = serverUrl + "/api/register-device";
    
    std::cout << "[DeviceRegistrar] Registering device with server..." << std::endl;
    int status = client.post(url, payload.dump(), adminJwt, response);
    
    if (status == 201) {
        try {
            auto resJson = nlohmann::json::parse(response);
            if (resJson.contains("token")) {
                tokenOut = resJson["token"].get<std::string>();
                std::cout << "[DeviceRegistrar] Device registration successful." << std::endl;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "[DeviceRegistrar] Failed to parse registration response: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[DeviceRegistrar] Device registration failed. HTTP Status: " << status << ", Response: " << response << std::endl;
    }
    return false;
}

bool DeviceRegistrar::registerDeviceWithKey(const std::string& serverUrl, const std::string& deviceName, const std::string& registrationKey, std::string& tokenOut) {
    HttpClient client;
    nlohmann::json payload;
    payload["name"] = deviceName;
    payload["ip_address"] = "127.0.0.1"; // default local loopback
    payload["registration_key"] = registrationKey;

    std::string response;
    std::string url = serverUrl + "/api/register-device";

    std::cout << "[DeviceRegistrar] Registering device automatically using registration key..." << std::endl;
    int status = client.post(url, payload.dump(), "", response);

    if (status == 201) {
        try {
            auto resJson = nlohmann::json::parse(response);
            if (resJson.contains("token")) {
                tokenOut = resJson["token"].get<std::string>();
                std::cout << "[DeviceRegistrar] Automatic registration successful." << std::endl;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "[DeviceRegistrar] Failed to parse registration response: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[DeviceRegistrar] Automatic registration failed. HTTP Status: " << status << ", Response: " << response << std::endl;
    }
    return false;
}
