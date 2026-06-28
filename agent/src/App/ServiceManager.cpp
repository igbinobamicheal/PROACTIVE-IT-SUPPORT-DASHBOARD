#include "App/ServiceManager.h"
#include "Infrastructure/Logging/Logger.h"
#include "Infrastructure/Config/JsonConfigRepository.h"
#include "Infrastructure/Metrics/WindowsMetricCollector.h"
#include "Infrastructure/Network/CprHttpSender.h"
#include "Application/AgentDaemon.h"
#include <filesystem>
#include <memory>

namespace App {

static std::unique_ptr<Application::AgentDaemon> g_Daemon = nullptr;

ServiceManager::ServiceManager()
    : statusHandle_(nullptr),
      status_({}),
      isRunning_(false) {
    instance_ = this;
}

ServiceManager& ServiceManager::GetInstance() {
    static ServiceManager instance;
    return instance;
}

bool ServiceManager::InstallService() {
    using namespace Infrastructure::Logging;

    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        Logger::GetInstance().Error("Failed to get current executable path.");
        return false;
    }

    // Ensure the installation directory exists
    std::filesystem::path installDir("C:\\Program Files\\ProactiveITAgent");
    try {
        std::filesystem::create_directories(installDir);
    } catch (const std::exception& e) {
        Logger::GetInstance().Warning("Failed to create install directory: " + std::string(e.what()));
    }

    SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        Logger::GetInstance().Error("Failed to open Service Control Manager. (Are you running as Administrator?)");
        return false;
    }

    SC_HANDLE hService = CreateServiceW(
        hSCManager,
        L"ProactiveITAgent",
        L"Proactive IT Support Agent",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePath,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!hService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            Logger::GetInstance().Warning("Service already exists.");
            CloseServiceHandle(hSCManager);
            return true;
        }
        Logger::GetInstance().Error("Failed to create service. Error code: " + std::to_string(err));
        CloseServiceHandle(hSCManager);
        return false;
    }

    // Add service description
    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = const_cast<LPWSTR>(L"Monitors system metrics and reports them to the Proactive IT Support Dashboard.");
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

    // Configure failure recovery options (restart automatically on crash)
    if (ConfigureRecoveryOptions(hService)) {
        Logger::GetInstance().Info("Configured service auto-restart recovery options on crash.");
    } else {
        Logger::GetInstance().Warning("Failed to configure service recovery options. Error code: " + std::to_string(GetLastError()));
    }

    Logger::GetInstance().Info("ProactiveITAgent service installed successfully.");
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

bool ServiceManager::UninstallService() {
    using namespace Infrastructure::Logging;

    SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        Logger::GetInstance().Error("Failed to open Service Control Manager.");
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, L"ProactiveITAgent", DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            Logger::GetInstance().Warning("Service does not exist.");
            CloseServiceHandle(hSCManager);
            return true;
        }
        Logger::GetInstance().Error("Failed to open service. Error code: " + std::to_string(err));
        CloseServiceHandle(hSCManager);
        return false;
    }

    // Try to stop the service first if it is running
    SERVICE_STATUS status;
    if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        Logger::GetInstance().Info("Stopping service before uninstallation...");
        Sleep(1000);
    }

    if (!DeleteService(hService)) {
        Logger::GetInstance().Error("Failed to delete service. Error code: " + std::to_string(GetLastError()));
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return false;
    }

    Logger::GetInstance().Info("ProactiveITAgent service uninstalled successfully.");
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

bool ServiceManager::RunService() {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { const_cast<LPWSTR>(L"ProactiveITAgent"), (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { nullptr, nullptr }
    };

    return StartServiceCtrlDispatcherW(serviceTable) != 0;
}

void WINAPI ServiceManager::ServiceMain(DWORD argc, LPWSTR* argv) {
    (void)argc; (void)argv;
    
    ServiceManager& self = GetInstance();
    
    // Initialize the logger to the C:\Program Files\ProactiveITAgent\agent.log file
    Infrastructure::Logging::Logger::GetInstance().Initialize("C:\\Program Files\\ProactiveITAgent\\agent.log");
    Infrastructure::Logging::Logger::GetInstance().Info("ServiceMain started.");

    self.statusHandle_ = RegisterServiceCtrlHandlerExW(L"ProactiveITAgent", CtrlHandler, &self);
    if (!self.statusHandle_) {
        Infrastructure::Logging::Logger::GetInstance().Error("Failed to register service control handler.");
        return;
    }

    self.status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    self.status_.dwServiceSpecificExitCode = 0;
    self.UpdateStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Initialize core daemon dependencies using clean architecture
    auto configRepo = std::make_unique<Infrastructure::Config::JsonConfigRepository>("C:\\Program Files\\ProactiveITAgent\\config.json");
    auto metricCollector = std::make_unique<Infrastructure::Metrics::WindowsMetricCollector>();
    auto httpSender = std::make_unique<Infrastructure::Network::CprHttpSender>();

    g_Daemon = std::make_unique<Application::AgentDaemon>(
        std::move(configRepo), 
        std::move(metricCollector), 
        std::move(httpSender),
        "C:\\Program Files\\ProactiveITAgent\\offline_metrics.json"
    );

    self.UpdateStatus(SERVICE_RUNNING);
    self.isRunning_ = true;

    Infrastructure::Logging::Logger::GetInstance().Info("ProactiveITAgent service is now running.");
    
    if (!g_Daemon->Start()) {
        Infrastructure::Logging::Logger::GetInstance().Error("Failed to start AgentDaemon.");
        self.UpdateStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
        return;
    }

    // Keep the service thread alive while running
    while (self.isRunning_) {
        Sleep(1000);
    }

    Infrastructure::Logging::Logger::GetInstance().Info("ServiceMain ending.");
}

DWORD WINAPI ServiceManager::CtrlHandler(DWORD ctrl, DWORD eventType, LPVOID eventData, LPVOID context) {
    (void)eventType; (void)eventData;
    ServiceManager* self = static_cast<ServiceManager*>(context);

    switch (ctrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            Infrastructure::Logging::Logger::GetInstance().Info("Received stop/shutdown signal from SCM.");
            self->UpdateStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
            
            if (g_Daemon) {
                g_Daemon->Stop();
            }
            
            self->isRunning_ = false;
            self->UpdateStatus(SERVICE_STOPPED);
            break;
            
        default:
            break;
    }
    return NO_ERROR;
}

void ServiceManager::UpdateStatus(DWORD state, DWORD exitCode, DWORD waitHint) {
    status_.dwCurrentState = state;
    status_.dwWin32ExitCode = exitCode;
    status_.dwWaitHint = waitHint;

    if (state == SERVICE_START_PENDING) {
        status_.dwControlsAccepted = 0;
    } else {
        status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
        status_.dwCheckPoint = 0;
    } else {
        static DWORD checkPoint = 1;
        status_.dwCheckPoint = checkPoint++;
    }

    SetServiceStatus(statusHandle_, &status_);
}

bool ServiceManager::ConfigureRecoveryOptions(SC_HANDLE hService) {
    SERVICE_FAILURE_ACTIONSW sfa = {};
    sfa.dwResetPeriod = 86400; // 1 day reset period
    sfa.lpRebootMsg = nullptr;
    sfa.lpCommand = nullptr;
    sfa.cActions = 3;

    // Failures actions array
    static SC_ACTION actions[3];
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 5000;  // restart after 5s
    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 10000; // restart after 10s
    actions[2].Type = SC_ACTION_RESTART;
    actions[2].Delay = 15000; // restart after 15s

    sfa.lpsaActions = actions;

    return ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa) != 0;
}

} // namespace App
