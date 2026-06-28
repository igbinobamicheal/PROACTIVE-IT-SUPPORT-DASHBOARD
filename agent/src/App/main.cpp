#include "App/ServiceManager.h"
#include "Infrastructure/Logging/Logger.h"
#include "Infrastructure/Config/JsonConfigRepository.h"
#include "Infrastructure/Metrics/WindowsMetricCollector.h"
#include "Infrastructure/Network/CprHttpSender.h"
#include "Application/AgentDaemon.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <filesystem>
#include <fstream>

std::string GetConfigPath() {
    if (std::filesystem::exists("config.json")) {
        return "config.json";
    }
    std::error_code ec;
    std::filesystem::path pfDir("C:\\Program Files\\ProactiveITAgent");
    bool pfWritable = false;
    if (std::filesystem::exists(pfDir, ec)) {
        std::filesystem::path dummy = pfDir / "dummy.tmp";
        std::ofstream f(dummy);
        if (f.is_open()) {
            f.close();
            std::filesystem::remove(dummy, ec);
            pfWritable = true;
        }
    }
    return pfWritable ? "C:\\Program Files\\ProactiveITAgent\\config.json" : "config.json";
}

std::string GetLogPath() {
    std::error_code ec;
    std::filesystem::path pfDir("C:\\Program Files\\ProactiveITAgent");
    bool pfWritable = false;
    if (std::filesystem::exists(pfDir, ec)) {
        std::filesystem::path dummy = pfDir / "dummy.tmp";
        std::ofstream f(dummy);
        if (f.is_open()) {
            f.close();
            std::filesystem::remove(dummy, ec);
            pfWritable = true;
        }
    }
    return pfWritable ? "C:\\Program Files\\ProactiveITAgent\\agent.log" : "agent.log";
}

void PrintUsage() {
    std::cout << "=== Proactive IT Support Agent ===\n"
              << "Usage CLI options (requires Administrator privileges):\n"
              << "  --install                            Install as a Windows Service\n"
              << "  --uninstall                          Uninstall the Windows Service\n"
              << "  --register <server_url> <reg_key>    Register this device with the server and write config\n"
              << "  --run                                Run interactively in console debug mode\n"
              << "  --help, -h                           Show this usage message\n";
}

int RunInteractive() {
    using namespace Infrastructure::Logging;
    
    Logger::GetInstance().Initialize(GetLogPath());
    Logger::GetInstance().Info("Starting agent in interactive console mode.");

    std::string configPath = GetConfigPath();
    std::filesystem::path p(configPath);
    std::string cachePath = (p.parent_path() / "offline_metrics.json").string();

    auto configRepo = std::make_unique<Infrastructure::Config::JsonConfigRepository>(configPath);
    auto metricCollector = std::make_unique<Infrastructure::Metrics::WindowsMetricCollector>();
    auto httpSender = std::make_unique<Infrastructure::Network::CprHttpSender>();

    Application::AgentDaemon daemon(
        std::move(configRepo), 
        std::move(metricCollector), 
        std::move(httpSender),
        cachePath
    );

    if (!daemon.Start()) {
        Logger::GetInstance().Error("Failed to start agent daemon.");
        return 1;
    }

    std::cout << "\nAgent is running. Press ENTER to stop the agent...\n";
    std::cin.get();

    daemon.Stop();
    return 0;
}

int RunRegister(const std::string& serverUrl, const std::string& regKey) {
    using namespace Infrastructure::Logging;
    
    // For registration CLI tool, log to console
    Logger::GetInstance().Initialize(GetLogPath());
    Logger::GetInstance().Info("Starting device registration...");

    Infrastructure::Metrics::WindowsMetricCollector collector;
    auto metrics = collector.Collect();
    
    Logger::GetInstance().Info("Detected system details:");
    Logger::GetInstance().Info("  Hostname: " + metrics.hostname);
    Logger::GetInstance().Info("  Local IP: " + metrics.local_ip);

    Infrastructure::Network::CprHttpSender sender;
    int deviceId = 0;
    std::string agentSecret;

    if (!sender.RegisterDevice(serverUrl, metrics, regKey, deviceId, agentSecret)) {
        Logger::GetInstance().Error("Device registration failed. See errors above.");
        return 1;
    }

    // Save to config file
    Infrastructure::Config::JsonConfigRepository repo(GetConfigPath());
    Domain::Models::AgentConfig config;
    config.server_url = serverUrl;
    config.device_id = deviceId;
    config.agent_secret = agentSecret;

    if (!repo.Save(config)) {
        Logger::GetInstance().Error("Failed to save registered device configuration to config.json.");
        return 1;
    }

    Logger::GetInstance().Info("Device registered successfully and configuration saved.");
    std::cout << "\nDevice ID: " << deviceId << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    // Collect arguments
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        // If run with no arguments, first check if we are launched by the SCM (Service Control Manager)
        App::ServiceManager& sm = App::ServiceManager::GetInstance();
        if (sm.RunService()) {
            return 0;
        }

        // If not launched by SCM, show usage help
        PrintUsage();
        return 0;
    }

    std::string action = args[0];

    if (action == "--install") {
        return App::ServiceManager::GetInstance().InstallService() ? 0 : 1;
    } 
    else if (action == "--uninstall") {
        return App::ServiceManager::GetInstance().UninstallService() ? 0 : 1;
    } 
    else if (action == "--register") {
        if (args.size() < 3) {
            std::cerr << "Error: --register requires <server_url> and <registration_key>.\n";
            PrintUsage();
            return 1;
        }
        return RunRegister(args[1], args[2]);
    } 
    else if (action == "--run") {
        return RunInteractive();
    } 
    else if (action == "--help" || action == "-h") {
        PrintUsage();
        return 0;
    } 
    else {
        std::cerr << "Unknown argument: " << action << "\n";
        PrintUsage();
        return 1;
    }
}
