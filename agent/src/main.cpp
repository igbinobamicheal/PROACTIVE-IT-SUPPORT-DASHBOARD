#include "ConfigManager.h"
#include "DeviceRegistrar.h"
#include "AgentService.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>

void configureStartup() {
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        std::cerr << "[Agent Warning] Failed to get current executable path." << std::endl;
        return;
    }

    HKEY hKey;
    LONG lResult = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_WRITE,
        &hKey
    );

    if (lResult == ERROR_SUCCESS) {
        lResult = RegSetValueExA(
            hKey,
            "ProactiveITAgent",
            0,
            REG_SZ,
            (const BYTE*)exePath,
            (DWORD)(strlen(exePath) + 1)
        );

        if (lResult == ERROR_SUCCESS) {
            std::cout << "[Agent] Configured to start automatically on Windows startup." << std::endl;
        } else {
            std::cerr << "[Agent Warning] Failed to write startup registry key. Error code: " << lResult << std::endl;
        }
        RegCloseKey(hKey);
    } else {
        std::cerr << "[Agent Warning] Failed to open startup registry key. Error code: " << lResult << std::endl;
    }
}


int main(int argc, char* argv[]) {
    std::cout << "=== Proactive IT Support - Windows Monitoring Agent ===" << std::endl;
    
    std::string configPath = "config.json";
    auto& config = ConfigManager::getInstance();
    
    // Attempt to load configuration, or generate a default template
    if (!config.load(configPath)) {
        std::cout << "[Agent] Config file not found. Creating default config.json" << std::endl;
        config.serverUrl = "http://localhost:8080";
        config.deviceName = "Windows_Monitoring_Agent";
        config.deviceToken = "";
        config.intervalSeconds = 10;
        config.adminUsername = "admin";
        config.adminPassword = "admin123";
        config.save(configPath);
        std::cout << "[Agent] Default config created. Edit config.json if needed." << std::endl;
    }

    // Configure the agent to start on Windows startup
    configureStartup();

    // Override generic device name with the local computer name
    if (config.deviceName == "Windows_Monitoring_Agent" || config.deviceName == "My_Agent_Device" || config.deviceName.empty()) {
        char buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buffer);
        if (GetComputerNameA(buffer, &size)) {
            config.deviceName = std::string(buffer);
            std::cout << "[Agent] Detected host computer name: " << config.deviceName << std::endl;
        }
    }

    // Check if the agent needs registration
    if (config.deviceToken.empty()) {
        std::cout << "[Agent] No device token found. Attempting registration..." << std::endl;
        DeviceRegistrar registrar;
        std::string generatedToken;
        bool registered = false;

        // Try registering automatically using the registration token from config
        if (!config.registrationKey.empty()) {
            std::cout << "[Agent] Found registration token. Attempting auto-registration..." << std::endl;
            if (registrar.registerDeviceWithKey(config.serverUrl, config.deviceName, config.registrationKey, generatedToken)) {
                config.deviceToken = generatedToken;
                config.registrationKey = ""; // Clear used token
                config.save(configPath);
                std::cout << "[Agent] Device registered successfully using registration token." << std::endl;
                registered = true;
            } else {
                std::cout << "[Agent Warning] Auto-registration failed. Token may be expired or already used." << std::endl;
            }
        }

        // Fallback: prompt for a registration token interactively
        if (!registered) {
            std::cout << "\n--- Registration Required ---" << std::endl;
            std::cout << "A registration token is required to register this agent." << std::endl;
            std::cout << "Generate one from the IT Dashboard under Devices > Registration Tokens." << std::endl;
            std::cout << "\nEnter Registration Token: ";
            
            std::string regToken;
            std::getline(std::cin, regToken);

            if (regToken.empty()) {
                std::cerr << "[Agent Error] Registration token is required. Exiting." << std::endl;
                return 1;
            }

            if (registrar.registerDeviceWithKey(config.serverUrl, config.deviceName, regToken, generatedToken)) {
                config.deviceToken = generatedToken;
                config.save(configPath);
                std::cout << "[Agent] Device registered successfully and token saved." << std::endl;
            } else {
                std::cerr << "[Agent Error] Failed to register device with the server. Exiting." << std::endl;
                return 1;
            }
        }
    }

    // Start background agent service
    AgentService service;
    service.start(configPath);

    std::cout << "\n[Agent] Running... Send metrics every " << config.intervalSeconds << " seconds." << std::endl;
    std::cout << "Press ENTER to stop the agent." << std::endl;
    std::cin.get();

    service.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[Agent] Exited." << std::endl;

    return 0;
}
