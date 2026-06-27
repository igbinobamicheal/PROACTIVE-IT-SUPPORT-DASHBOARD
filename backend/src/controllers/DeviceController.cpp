#include "controllers/DeviceController.hpp"
#include "repositories/DeviceRepository.hpp"
#include "repositories/MetricRepository.hpp"
#include "config/Config.hpp"
#include "utils/JWTUtil.hpp"
#include "utils/EventBroker.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <filesystem>
#include <queue>

void DeviceController::registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app) {
    // 1. POST /api/register-device (unguarded by AuthMiddleware, verified internally)
    CROW_ROUTE(app, "/api/register-device")
    .methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            auto body = nlohmann::json::parse(req.body);
            
            // Check authorization: either a valid registration key or admin JWT
            bool isAuthorized = false;
            
            // A. Check for registration key in body
            if (body.contains("registration_key")) {
                std::string key = body["registration_key"];
                if (key == Config::getInstance().registrationKey) {
                    isAuthorized = true;
                }
            }
            
            // B. Check for user JWT in Authorization header
            if (!isAuthorized) {
                std::string authHeader = req.get_header_value("Authorization");
                if (!authHeader.empty() && authHeader.find("Bearer ") == 0) {
                    std::string token = authHeader.substr(7);
                    std::string username = JWTUtil::verifyToken(token, Config::getInstance().jwtSecret);
                    if (!username.empty()) {
                        isAuthorized = true;
                    }
                }
            }
            
            if (!isAuthorized) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Unauthorized: Valid Authorization token or registration_key is required"}}.dump());
                return res;
            }

            if (!body.contains("name") || !body.contains("ip_address")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Device name and ip_address are required"}}.dump());
                return res;
            }

            Device dev;
            dev.name = body["name"];
            dev.ipAddress = body["ip_address"];
            dev.status = "offline"; // Initial status is offline until first metric is received

            DeviceRepository devRepo;
            devRepo.create(dev);

            res.code = 201;
            res.write(nlohmann::json(dev).dump());
            return res;

        } catch (const nlohmann::json::parse_error& e) {
            res.code = 400;
            res.write(nlohmann::json{{"error", "Malformed JSON request body"}}.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Internal server error: ") + e.what()}}.dump());
            return res;
        }
    });

    // 1b. GET /api/deploy/agent — serves a PowerShell install script (works on Linux/Railway)
    CROW_ROUTE(app, "/api/deploy/agent")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        try {
            auto& config = Config::getInstance();

            // Determine the public backend URL from forwarded headers (set by Railway)
            std::string proto = req.get_header_value("X-Forwarded-Proto");
            if (proto.empty()) proto = "https";
            std::string host = req.get_header_value("X-Forwarded-Host");
            if (host.empty()) host = req.get_header_value("Host");
            if (host.empty()) host = "localhost:8080";

            std::string serverUrl = proto + "://" + host;
            std::string registrationKey = config.registrationKey;

            // Read AGENT_DOWNLOAD_URL from environment (set in Railway variables)
            const char* agentUrlEnv = std::getenv("AGENT_DOWNLOAD_URL");
            std::string agentDownloadUrl = agentUrlEnv ? agentUrlEnv : "";

            if (agentDownloadUrl.empty()) {
                res.code = 503;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "AGENT_DOWNLOAD_URL environment variable is not set. Upload the agent .exe to GitHub Releases and set this variable in Railway."}}.dump());
                return res;
            }

            // Build the PowerShell install script dynamically
            std::string ps1 = R"raw(
$ErrorActionPreference = "Stop"
$InstallDir = "$env:ProgramData\ProactiveITAgent"
$ExePath    = "$InstallDir\proactive_it_agent.exe"
$CfgPath    = "$InstallDir\config.json"

Write-Host "=== Proactive IT Agent Installer ===" -ForegroundColor Cyan

if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Write-Host "[+] Created install directory: $InstallDir"
}

Write-Host "[*] Downloading agent from: {{AGENT_DOWNLOAD_URL}}"
Invoke-WebRequest -Uri "{{AGENT_DOWNLOAD_URL}}" -OutFile $ExePath -UseBasicParsing
Write-Host "[+] Agent downloaded."

$config = @{
    server           = "{{SERVER_URL}}/api"
    device_name      = $env:COMPUTERNAME
    device_token     = ""
    interval_seconds = 10
    registration_key = "{{REGISTRATION_KEY}}"
} | ConvertTo-Json
Set-Content -Path $CfgPath -Value $config -Encoding UTF8
Write-Host "[+] Config written: $CfgPath"

$taskName = "ProactiveITAgent"
$action   = New-ScheduledTaskAction -Execute $ExePath -WorkingDirectory $InstallDir
$trigger  = New-ScheduledTaskTrigger -AtStartup
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 0) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
$principal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -LogonType ServiceAccount -RunLevel Highest
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Force | Out-Null
Write-Host "[+] Scheduled task registered: $taskName (runs as SYSTEM on startup)"

Write-Host "[*] Starting agent now..."
Start-ScheduledTask -TaskName $taskName
Write-Host "[+] Agent started! It will appear in your dashboard within 15 seconds." -ForegroundColor Green
Write-Host ""
Write-Host "To uninstall: Unregister-ScheduledTask -TaskName ProactiveITAgent -Confirm:$false; Remove-Item -Recurse $InstallDir"
)raw";

            auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
                size_t start_pos = 0;
                while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                    str.replace(start_pos, from.length(), to);
                    start_pos += to.length();
                }
            };

            replaceAll(ps1, "{{AGENT_DOWNLOAD_URL}}", agentDownloadUrl);
            replaceAll(ps1, "{{SERVER_URL}}", serverUrl);
            replaceAll(ps1, "{{REGISTRATION_KEY}}", registrationKey);

            res.code = 200;
            res.set_header("Content-Type", "text/plain; charset=utf-8");
            res.set_header("Content-Disposition", "inline; filename=\"install-agent.ps1\"");
            res.write(ps1);
            return res;

        } catch (const std::exception& e) {
            res.code = 500;
            res.set_header("Content-Type", "application/json");
            res.write(nlohmann::json{{"error", std::string("Failed to generate install script: ") + e.what()}}.dump());
            return res;
        }
    });

    // 2. GET /api/devices (guarded by JWT)
    CROW_ROUTE(app, "/api/devices")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            DeviceRepository devRepo;
            auto devices = devRepo.findAll();
            
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& dev : devices) {
                arr.push_back(dev);
            }

            res.code = 200;
            res.write(arr.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Internal server error: ") + e.what()}}.dump());
            return res;
        }
    });

    // 3. GET /api/device/{id} (guarded by JWT)
    CROW_ROUTE(app, "/api/device/<int>")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](int id) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            DeviceRepository devRepo;
            auto devOpt = devRepo.findById(id);

            if (!devOpt.has_value()) {
                res.code = 404;
                res.write(nlohmann::json{{"error", "Device not found"}}.dump());
                return res;
            }

            Device dev = devOpt.value();
            nlohmann::json responseJson = dev;

            MetricRepository metricRepo;
            auto metricOpt = metricRepo.findLatestForDevice(id);
            if (metricOpt.has_value()) {
                responseJson["latest_metrics"] = metricOpt.value();
            } else {
                responseJson["latest_metrics"] = nullptr;
            }

            res.code = 200;
            res.write(responseJson.dump());
            return res;

        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Internal server error: ") + e.what()}}.dump());
            return res;
        }
    });

    // 4. GET /api/device/{id}/metrics (guarded by JWT)
    CROW_ROUTE(app, "/api/device/<int>/metrics")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](int id) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            MetricRepository metricRepo;
            auto metrics = metricRepo.findHistoryForDevice(id, 20); // Retrieve last 20 measurements
            
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& metric : metrics) {
                arr.push_back(metric);
            }

            res.code = 200;
            res.write(arr.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Internal server error: ") + e.what()}}.dump());
            return res;
        }
    });

    // 5. GET /api/events (guarded by JWT, supports fallback token in query params)
    CROW_ROUTE(app, "/api/events")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req, crow::response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        auto queue = std::make_shared<std::queue<Event>>();
        Event ev;

        // Block thread for up to 15 seconds waiting for an event
        if (EventBroker::getInstance().waitForEvent(queue, ev, 15000)) {
            std::string sseMessage = "event: " + ev.type + "\n" + "data: " + ev.data + "\n\n";
            res.write(sseMessage);
        } else {
            res.write(": keepalive\n\n");
        }
        res.end();
    });
}

