#include "controllers/DeviceController.hpp"
#include "repositories/DeviceRepository.hpp"
#include "repositories/RegistrationTokenRepository.hpp"
#include "repositories/MetricRepository.hpp"
#include "repositories/DeviceUserRepository.hpp"
#include "config/Config.hpp"
#include "utils/JWTUtil.hpp"
#include "utils/EventBroker.hpp"
#include "database/Database.hpp"
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <filesystem>
#include <queue>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace {
std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4"; // UUIDv4 version
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen); // UUIDv4 variant
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

std::string getFormattedExpiry(int minutesFromNow) {
    auto now = std::chrono::system_clock::now() + std::chrono::minutes(minutesFromNow);
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#if defined(_MSC_VER)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::stringstream ss;
    ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
} // namespace

void DeviceController::registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app) {
    // 0. POST /api/registration-tokens (guarded by AuthMiddleware)
    CROW_ROUTE(app, "/api/registration-tokens")
    .methods(crow::HTTPMethod::POST)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            std::optional<int> assignedUserId;
            if (!req.body.empty()) {
                try {
                    auto body = nlohmann::json::parse(req.body);
                    if (body.contains("assigned_user_id") && !body["assigned_user_id"].is_null()) {
                        assignedUserId = body["assigned_user_id"].get<int>();
                    }
                } catch (...) {
                    // Ignore JSON parsing errors for compatibility with legacy requests
                }
            }

            std::string token = generateUUID();
            std::string expiresAt = getFormattedExpiry(15);
            
            RegistrationTokenRepository repo;
            repo.create(token, expiresAt, assignedUserId);
            
            nlohmann::json response = {
                {"token", token},
                {"expires_at", expiresAt}
            };
            if (assignedUserId.has_value()) {
                response["assigned_user_id"] = assignedUserId.value();
            } else {
                response["assigned_user_id"] = nullptr;
            }
            
            res.code = 201;
            res.write(response.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Failed to generate registration token: ") + e.what()}}.dump());
            return res;
        }
    });

    // 0b. GET /api/registration-tokens (guarded by AuthMiddleware)
    CROW_ROUTE(app, "/api/registration-tokens")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            RegistrationTokenRepository repo;
            auto tokens = repo.findAll();
            
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& t : tokens) {
                nlohmann::json item;
                item["id"] = t.id;
                item["token"] = t.token;
                item["used"] = t.used;
                item["is_expired"] = t.isExpired;
                if (t.assignedUserId.has_value()) {
                    item["assigned_user_id"] = t.assignedUserId.value();
                } else {
                    item["assigned_user_id"] = nullptr;
                }
                if (t.assignedUserFullName.has_value()) {
                    item["assigned_user_name"] = t.assignedUserFullName.value();
                } else {
                    item["assigned_user_name"] = nullptr;
                }
                if (t.assignedUserEmail.has_value()) {
                    item["assigned_user_email"] = t.assignedUserEmail.value();
                } else {
                    item["assigned_user_email"] = nullptr;
                }
                arr.push_back(item);
            }
            
            res.code = 200;
            res.write(arr.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Failed to fetch registration tokens: ") + e.what()}}.dump());
            return res;
        }
    });

    // 0c. POST /api/registration-tokens/revoke (guarded by AuthMiddleware)
    CROW_ROUTE(app, "/api/registration-tokens/revoke")
    .methods(crow::HTTPMethod::POST)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("token")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Missing token in request body"}}.dump());
                return res;
            }
            
            std::string tokenStr = body["token"];
            RegistrationTokenRepository repo;
            repo.revokeToken(tokenStr);
            
            res.code = 200;
            res.write(nlohmann::json{{"message", "Token revoked successfully"}}.dump());
            return res;
        } catch (const nlohmann::json::parse_error& e) {
            res.code = 400;
            res.write(nlohmann::json{{"error", "Malformed JSON request body"}}.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Failed to revoke token: ") + e.what()}}.dump());
            return res;
        }
    });

    // 1. POST /api/register-device (unguarded by AuthMiddleware, verified internally)
    CROW_ROUTE(app, "/api/register-device")
    .methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            auto body = nlohmann::json::parse(req.body);
            
            if (!body.contains("registration_token") || !body.contains("name") || 
                !body.contains("machine_guid") || !body.contains("mac_address") || 
                !body.contains("windows_version")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Missing required fields (registration_token, name, machine_guid, mac_address, windows_version)"}}.dump());
                return res;
            }

            std::string regToken = body["registration_token"];
            RegistrationTokenRepository tokenRepo;
            auto rtOpt = tokenRepo.findByToken(regToken);
            
            if (!rtOpt.has_value()) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Unauthorized: Invalid registration token"}}.dump());
                return res;
            }

            auto rt = rtOpt.value();
            if (rt.used) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Unauthorized: Registration token has already been used"}}.dump());
                return res;
            }

            if (rt.isExpired) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Unauthorized: Registration token has expired"}}.dump());
                return res;
            }

            // Invalidate the token immediately
            tokenRepo.useToken(regToken);

            std::string ip = req.remote_ip_address;
            if (ip.empty() || ip == "127.0.0.1" || ip == "::1") {
                if (body.contains("ip_address")) {
                    ip = body["ip_address"];
                } else {
                    ip = "127.0.0.1";
                }
            }

            DeviceRepository devRepo;
            std::string guid = body["machine_guid"];
            auto existingDevOpt = devRepo.findByGuid(guid);

            Device dev;
            if (existingDevOpt.has_value()) {
                // Update existing device
                dev = existingDevOpt.value();
                dev.name = body["name"];
                dev.ipAddress = ip;
                dev.macAddress = body["mac_address"];
                dev.windowsVersion = body["windows_version"];
                dev.status = "offline";
                
                // Generate a new unique device token (credentials refresh)
                std::random_device rd;
                std::mt19937 generator(rd());
                std::uniform_int_distribution<uint64_t> distribution;
                std::stringstream ss;
                ss << std::hex << std::setfill('0');
                ss << std::setw(16) << distribution(generator);
                ss << std::setw(16) << distribution(generator);
                ss << std::setw(16) << distribution(generator);
                ss << std::setw(16) << distribution(generator);
                dev.token = ss.str();
                
                devRepo.update(dev);
                if (rt.assignedUserId.has_value()) {
                    devRepo.assignUser(dev.id, rt.assignedUserId.value());
                }
            } else {
                // Create a new device
                dev.name = body["name"];
                dev.ipAddress = ip;
                dev.machineGuid = guid;
                dev.macAddress = body["mac_address"];
                dev.windowsVersion = body["windows_version"];
                dev.status = "offline";
                
                devRepo.create(dev); // Sets ID and Token automatically
                if (rt.assignedUserId.has_value()) {
                    devRepo.assignUser(dev.id, rt.assignedUserId.value());
                }
            }

            res.code = 201;
            res.write(nlohmann::json{
                {"id", dev.id},
                {"name", dev.name},
                {"token", dev.token},
                {"ip_address", dev.ipAddress},
                {"status", dev.status}
            }.dump());
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

    // 1b. GET /api/deploy/agent — serves a PowerShell install script (verified by token query param)
    CROW_ROUTE(app, "/api/deploy/agent")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
        crow::response res;
        try {
            // Get registration token from query string
            const char* tokenParam = req.url_params.get("token");
            if (!tokenParam || tokenParam[0] == '\0') {
                res.code = 401;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Missing token parameter in query string"}}.dump());
                return res;
            }

            std::string regToken(tokenParam);
            RegistrationTokenRepository tokenRepo;
            auto rtOpt = tokenRepo.findByToken(regToken);

            if (!rtOpt.has_value()) {
                res.code = 401;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Unauthorized: Invalid registration token"}}.dump());
                return res;
            }

            auto rt = rtOpt.value();
            if (rt.used) {
                res.code = 401;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Unauthorized: Registration token has already been used"}}.dump());
                return res;
            }

            if (rt.isExpired) {
                res.code = 401;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Unauthorized: Registration token has expired"}}.dump());
                return res;
            }

            // Determine the public backend URL
            std::string proto = req.get_header_value("X-Forwarded-Proto");
            if (proto.empty()) proto = "https";
            std::string host = req.get_header_value("X-Forwarded-Host");
            if (host.empty()) host = req.get_header_value("Host");
            if (host.empty()) host = "localhost:8080";

            std::string serverUrl = proto + "://" + host;

            // Read AGENT_DOWNLOAD_URL from environment
            const char* agentUrlEnv = std::getenv("AGENT_DOWNLOAD_URL");
            std::string agentDownloadUrl = agentUrlEnv ? agentUrlEnv : "";

            if (agentDownloadUrl.empty()) {
                res.code = 503;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "AGENT_DOWNLOAD_URL environment variable is not set. Upload the agent .exe to GitHub Releases and set this variable."}}.dump());
                return res;
            }

            // Build the PowerShell install script dynamically
            std::string ps1 = R"raw(
$ErrorActionPreference = "Stop"
$InstallDir = "C:\Program Files\ProactiveITAgent"
$ExePath    = "$InstallDir\proactive_it_agent.exe"

Write-Host "=== Proactive IT Agent Installer ===" -ForegroundColor Cyan

if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Write-Host "[+] Created install directory: $InstallDir"
}

Write-Host "[*] Downloading agent from: {{AGENT_DOWNLOAD_URL}}"
Invoke-WebRequest -Uri "{{AGENT_DOWNLOAD_URL}}" -OutFile $ExePath -UseBasicParsing
Write-Host "[+] Agent downloaded."

Write-Host "[*] Registering device with dashboard..."
& $ExePath --register "{{SERVER_URL}}" "{{REGISTRATION_TOKEN}}"
Write-Host "[+] Device registered and credentials saved."

Write-Host "[*] Installing Windows Service..."
& $ExePath --install
Write-Host "[+] Windows Service installed successfully."

Write-Host "[*] Starting monitoring service now..."
Start-Service ProactiveITAgent
Write-Host "[+] Agent started! It will appear in your dashboard within 15 seconds." -ForegroundColor Green
Write-Host ""
Write-Host "To uninstall: & '$ExePath' --uninstall; Remove-Item -Recurse -Force $InstallDir"
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
            replaceAll(ps1, "{{REGISTRATION_TOKEN}}", regToken);

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

    // 1c. GET /api/deploy/installer — serves the compiled Windows Installer executable
    CROW_ROUTE(app, "/api/deploy/installer")
    .methods(crow::HTTPMethod::GET)
    ([]() {
        crow::response res;
        try {
            const char* urlEnv = std::getenv("AGENT_DOWNLOAD_URL");
            std::string downloadUrl = urlEnv ? urlEnv : "";
            if (downloadUrl.empty()) {
                std::string localPath = "installer/windows/Output/ProactiveITAgentSetup.exe";
                if (std::filesystem::exists(localPath)) {
                    res.set_static_file_info(localPath);
                    res.code = 200;
                    return res;
                }
                
                // Fallback to checking local Release build of setup
                std::string localPathRelease = "installer/windows/ProactiveITAgentSetup.exe";
                if (std::filesystem::exists(localPathRelease)) {
                    res.set_static_file_info(localPathRelease);
                    res.code = 200;
                    return res;
                }

                res.code = 503;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Agent installer download URL is not set and local installer file is missing."}}.dump());
                return res;
            }
            res.code = 307;
            res.set_header("Location", downloadUrl);
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.set_header("Content-Type", "application/json");
            res.write(nlohmann::json{{"error", std::string("Failed to process installer download: ") + e.what()}}.dump());
            return res;
        }
    });

    // 1d. POST /api/heartbeat (unguarded by user JWT, verified internally)
    CROW_ROUTE(app, "/api/heartbeat")
    .methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        std::string deviceToken = req.get_header_value("X-Device-Token");
        if (deviceToken.empty()) {
            res.code = 401;
            res.write(nlohmann::json{{"error", "Missing X-Device-Token header"}}.dump());
            return res;
        }

        try {
            DeviceRepository devRepo;
            auto devOpt = devRepo.findByToken(deviceToken);
            if (!devOpt.has_value()) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Invalid device token"}}.dump());
                return res;
            }

            auto dev = devOpt.value();
            bool wentOnline = (dev.status == "offline");

            // Update status and last_seen
            {
                auto dbConn = Database::getInstance().getConnection();
                pqxx::work txn(*dbConn);
                txn.exec_prepared("update_device_status", "online", dev.id);
                txn.commit();
            }

            // Publish online event if status changed
            if (wentOnline) {
                nlohmann::json onlineEvent = {
                    {"device_id", dev.id},
                    {"device_name", dev.name},
                    {"ip_address", dev.ipAddress},
                    {"timestamp", EventBroker::getCurrentTimestamp()}
                };
                EventBroker::getInstance().publish("device_online", onlineEvent.dump());
            }

            res.code = 200;
            res.write(nlohmann::json{{"message", "Heartbeat received successfully"}}.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Internal server error: ") + e.what()}}.dump());
            return res;
        }
    });

    // 1e. GET /api/agent/latest — returns the latest version, download url, and checksum of the agent
    CROW_ROUTE(app, "/api/agent/latest")
    .methods(crow::HTTPMethod::GET)
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            const char* versionEnv = std::getenv("LATEST_AGENT_VERSION");
            const char* urlEnv = std::getenv("AGENT_DOWNLOAD_URL");
            const char* checksumEnv = std::getenv("LATEST_AGENT_CHECKSUM");

            nlohmann::json j;
            j["latest_version"] = versionEnv ? versionEnv : "2.0.0";
            j["download_url"] = urlEnv ? urlEnv : "";
            j["checksum"] = checksumEnv ? checksumEnv : "";

            res.code = 200;
            res.write(j.dump());
            return res;
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(nlohmann::json{{"error", std::string("Failed to fetch latest agent info: ") + e.what()}}.dump());
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

    // 6. GET /api/device-users (guarded by JWT)
    CROW_ROUTE(app, "/api/device-users")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([]() {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            DeviceUserRepository repo;
            auto users = repo.findAll();
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& u : users) {
                arr.push_back(u);
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

    // 7. POST /api/device-users (guarded by JWT)
    CROW_ROUTE(app, "/api/device-users")
    .methods(crow::HTTPMethod::POST)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("full_name") || !body.contains("email")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Missing full_name or email"}}.dump());
                return res;
            }

            DeviceUser u;
            u.fullName = body["full_name"];
            u.email = body["email"];
            if (body.contains("department")) {
                u.department = body["department"];
            }

            DeviceUserRepository repo;
            // Check if user already exists
            auto existing = repo.findByEmail(u.email);
            if (existing.has_value()) {
                res.code = 409;
                res.write(nlohmann::json{{"error", "User with this email already exists"}}.dump());
                return res;
            }

            repo.create(u);
            res.code = 201;
            res.write(nlohmann::json(u).dump());
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

    // 8. POST /api/device/<id>/assign (guarded by JWT)
    CROW_ROUTE(app, "/api/device/<int>/assign")
    .methods(crow::HTTPMethod::POST)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req, int deviceId) {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        try {
            auto body = nlohmann::json::parse(req.body);
            std::optional<int> userId;
            if (body.contains("user_id") && !body["user_id"].is_null()) {
                userId = body["user_id"].get<int>();
            }

            DeviceRepository devRepo;
            auto devOpt = devRepo.findById(deviceId);
            if (!devOpt.has_value()) {
                res.code = 404;
                res.write(nlohmann::json{{"error", "Device not found"}}.dump());
                return res;
            }

            if (userId.has_value()) {
                DeviceUserRepository userRepo;
                auto userOpt = userRepo.findById(userId.value());
                if (!userOpt.has_value()) {
                    res.code = 404;
                    res.write(nlohmann::json{{"error", "Assigned user not found"}}.dump());
                    return res;
                }
            }

            devRepo.assignUser(deviceId, userId);

            // Fetch the updated device to return
            auto updatedDevOpt = devRepo.findById(deviceId);
            res.code = 200;
            if (updatedDevOpt.has_value()) {
                // Publish update event so UI updates immediately via SSE
                nlohmann::json devJson = updatedDevOpt.value();
                nlohmann::json updateEvent = {
                    {"device_id", deviceId},
                    {"assigned_user_id", userId.has_value() ? nlohmann::json(userId.value()) : nlohmann::json(nullptr)},
                    {"assigned_user_name", userId.has_value() ? nlohmann::json(updatedDevOpt.value().assignedUserFullName.value()) : nlohmann::json(nullptr)},
                    {"assigned_user_email", userId.has_value() ? nlohmann::json(updatedDevOpt.value().assignedUserEmail.value()) : nlohmann::json(nullptr)},
                    {"assigned_user_dept", userId.has_value() ? nlohmann::json(updatedDevOpt.value().assignedUserDepartment.value()) : nlohmann::json(nullptr)}
                };
                EventBroker::getInstance().publish("device_user_assigned", updateEvent.dump());

                res.write(devJson.dump());
            } else {
                res.write(nlohmann::json{{"message", "Device assignment updated successfully"}}.dump());
            }
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
}

