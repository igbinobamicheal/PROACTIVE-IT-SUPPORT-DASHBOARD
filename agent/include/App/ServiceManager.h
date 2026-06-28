#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <atomic>

namespace App {

class ServiceManager {
public:
    static ServiceManager& GetInstance();

    bool InstallService();
    bool UninstallService();
    bool RunService();

private:
    ServiceManager();
    ~ServiceManager() = default;
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

    static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
    static DWORD WINAPI CtrlHandler(DWORD ctrl, DWORD eventType, LPVOID eventData, LPVOID context);

    void UpdateStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0);
    bool ConfigureRecoveryOptions(SC_HANDLE hService);

    SERVICE_STATUS_HANDLE statusHandle_;
    SERVICE_STATUS status_;
    std::atomic<bool> isRunning_;
    
    static inline ServiceManager* instance_ = nullptr;
};

} // namespace App
