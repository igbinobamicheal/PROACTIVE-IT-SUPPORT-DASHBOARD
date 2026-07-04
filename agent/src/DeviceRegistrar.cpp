#include "DeviceRegistrar.h"
#include "HttpClient.h"
#include <nlohmann/json.hpp>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ntdll.lib")

// Collect the machine's unique GUID from the Windows registry
static std::string getMachineGuid() {
    HKEY hKey;
    char buffer[256] = {0};
    DWORD bufferSize = sizeof(buffer);
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)buffer, &bufferSize);
        RegCloseKey(hKey);
    }
    return std::string(buffer);
}

// Collect the primary MAC address from the first active network adapter
static std::string getMacAddress() {
    ULONG bufLen = 0;
    GetAdaptersInfo(nullptr, &bufLen);
    
    std::vector<BYTE> buf(bufLen);
    PIP_ADAPTER_INFO pAdapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
    
    if (GetAdaptersInfo(pAdapterInfo, &bufLen) == NO_ERROR) {
        std::ostringstream oss;
        for (UINT i = 0; i < pAdapterInfo->AddressLength; ++i) {
            if (i > 0) oss << ":";
            oss << std::hex << std::setfill('0') << std::setw(2) << (int)pAdapterInfo->Address[i];
        }
        return oss.str();
    }
    return "00:00:00:00:00:00";
}

// Collect the Windows version string
static std::string getWindowsVersion() {
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

    // Use RtlGetVersion to bypass compatibility shims
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    RTL_OSVERSIONINFOW rovi = {0};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        auto fn = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (fn && fn(&rovi) == 0) {
            std::ostringstream oss;
            oss << "Windows " << rovi.dwMajorVersion << "." << rovi.dwMinorVersion << " (Build " << rovi.dwBuildNumber << ")";
            return oss.str();
        }
    }
    return "Windows (unknown version)";
}

// Build the common registration payload with system identity fields
static nlohmann::json buildRegistrationPayload(const std::string& deviceName) {
    nlohmann::json payload;
    payload["name"] = deviceName;
    payload["ip_address"] = "127.0.0.1"; // Will be overridden by backend with remote IP
    payload["machine_guid"] = getMachineGuid();
    payload["mac_address"] = getMacAddress();
    payload["windows_version"] = getWindowsVersion();
    return payload;
}

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
    
    // This path requires a registration token generated from the admin dashboard.
    // The admin JWT alone is not sufficient — a registration_token must be created first.
    std::cerr << "[DeviceRegistrar] JWT-based registration requires a registration token." << std::endl;
    std::cerr << "[DeviceRegistrar] Please generate a registration token from the dashboard and use key-based registration instead." << std::endl;
    return false;
}

bool DeviceRegistrar::registerDeviceWithKey(const std::string& serverUrl, const std::string& deviceName, const std::string& registrationKey, std::string& tokenOut) {
    HttpClient client;
    nlohmann::json payload = buildRegistrationPayload(deviceName);
    payload["registration_token"] = registrationKey; // Backend expects "registration_token", not "registration_key"

    std::string response;
    std::string url = serverUrl + "/api/register-device";

    std::cout << "[DeviceRegistrar] Registering device automatically using registration token..." << std::endl;
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

