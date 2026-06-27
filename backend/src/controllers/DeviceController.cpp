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

    // 1b. GET /api/deploy/agent (guarded by JWT)
    CROW_ROUTE(app, "/api/deploy/agent")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        
#ifdef _WIN32
        try {
            auto& config = Config::getInstance();
            std::string host = req.get_header_value("Host");
            if (host.empty()) {
                host = "localhost:8080";
            }
            
            std::string proto = req.get_header_value("X-Forwarded-Proto");
            if (proto.empty()) {
                proto = "http";
            }
            
            // Construct agent's config.json content
            nlohmann::json agentConfig = {
                {"server_url", proto + "://" + host},
                {"registration_key", config.registrationKey}
            };
            
            // Write config.json to backend/uploads
            std::string uploadsDir = "../uploads"; // relative to backend/config
            std::string configPath = uploadsDir + "/config.json";
            
            // Ensure uploads directory exists (just in case)
            std::string mkdirCmd = "powershell -Command \"New-Item -ItemType Directory -Force -Path '" + uploadsDir + "'\"";
            std::system(mkdirCmd.c_str());
            
            std::ofstream configFile(configPath);
            if (!configFile.is_open()) {
                res.code = 500;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Failed to create config.json in uploads directory"}}.dump());
                return res;
            }
            configFile << agentConfig.dump(4);
            configFile.close();
            
            // Find proactive_it_agent.exe
            std::string agentSource = "";
            if (std::ifstream("../../agent/build/Release/proactive_it_agent.exe")) {
                agentSource = "../../agent/build/Release/proactive_it_agent.exe";
            } else if (std::ifstream("../../agent/build/Debug/proactive_it_agent.exe")) {
                agentSource = "../../agent/build/Debug/proactive_it_agent.exe";
            } else if (std::ifstream("proactive_it_agent.exe")) {
                agentSource = "proactive_it_agent.exe";
            }
            
            if (agentSource.empty()) {
                std::remove(configPath.c_str());
                res.code = 500;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Agent executable not found. Make sure the agent is compiled first."}}.dump());
                return res;
            }
            
            // Copy agent.exe to uploads/agent.exe
            std::string agentDest = uploadsDir + "/agent.exe";
            std::ifstream src(agentSource, std::ios::binary);
            std::ofstream dst(agentDest, std::ios::binary);
            if (!src.is_open() || !dst.is_open()) {
                std::remove(configPath.c_str());
                res.code = 500;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Failed to copy agent executable to packaging folder"}}.dump());
                return res;
            }
            dst << src.rdbuf();
            src.close();
            dst.close();

            // Dynamic DLL copying using C++17 <filesystem>
            std::vector<std::string> copiedDlls;
            try {
                namespace fs = std::filesystem;
                std::string sourceDir = agentSource.substr(0, agentSource.find_last_of("/\\"));
                if (sourceDir.empty()) sourceDir = ".";
                
                for (const auto& entry : fs::directory_iterator(sourceDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                        std::string dllName = entry.path().filename().string();
                        std::string destPath = uploadsDir + "/" + dllName;
                        fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing);
                        copiedDlls.push_back(destPath);
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "[Deployment] Warning: Failed to scan/copy agent DLL dependencies: " << ex.what() << std::endl;
            }
            
            // Create ZIP package using PowerShell (native on Windows)
            std::string zipPath = uploadsDir + "/agent_package.zip";
            // Clean up any old zip
            std::remove(zipPath.c_str());
            
            // Build the powershell Compress-Archive file list
            std::string fileList = "'" + agentDest + "', '" + configPath + "'";
            for (const auto& dll : copiedDlls) {
                fileList += ", '" + dll + "'";
            }
            
            std::string cmd = "powershell -Command \"Compress-Archive -Path " + fileList + " -DestinationPath '" + zipPath + "' -Force\"";
            int ret = std::system(cmd.c_str());
            
            // Cleanup copied source files immediately
            std::remove(configPath.c_str());
            std::remove(agentDest.c_str());
            for (const auto& dll : copiedDlls) {
                std::remove(dll.c_str());
            }
            
            if (ret != 0) {
                res.code = 500;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Failed to compress package: Command returned " + std::to_string(ret)}}.dump());
                return res;
            }
            
            // Read zip file into response
            std::ifstream zipFile(zipPath, std::ios::binary);
            if (!zipFile.is_open()) {
                res.code = 500;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Failed to open generated ZIP file"}}.dump());
                return res;
            }
            
            std::string zipData((std::istreambuf_iterator<char>(zipFile)), std::istreambuf_iterator<char>());
            zipFile.close();
            
            // Clean up ZIP file from disk
            std::remove(zipPath.c_str());
            
            res.code = 200;
            res.set_header("Content-Type", "application/zip");
            res.set_header("Content-Disposition", "attachment; filename=\"agent_package.zip\"");
            res.write(zipData);
            return res;
            
        } catch (const std::exception& e) {
            res.code = 500;
            res.set_header("Content-Type", "application/json");
            res.write(nlohmann::json{{"error", std::string("Deployment failed: ") + e.what()}}.dump());
            return res;
        }
#else
        // Agent deployment packages require Windows (PowerShell + .exe).
        // On Linux (Railway), return a descriptive error.
        res.code = 501;
        res.set_header("Content-Type", "application/json");
        res.write(nlohmann::json{{"error", "Agent deployment packages are only available when the server is hosted on Windows."}}.dump());
        return res;
#endif
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

