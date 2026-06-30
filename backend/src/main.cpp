#include "config/Config.hpp"
#include "database/Database.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/DeviceController.hpp"
#include "controllers/MetricController.hpp"
#include "controllers/AlertController.hpp"
#include "middleware/CORSMiddleware.hpp"
#include "utils/EventBroker.hpp"
#include <crow.h>
#include <iostream>
#include <exception>
#include <thread>
#include <atomic>
#include <chrono>

#include <pqxx/pqxx>

std::atomic<bool> g_stopStatusChecker(false);
std::thread g_statusCheckerThread;

void runStatusChecker() {
    std::cout << "[StatusChecker] Background service started." << std::endl;
    while (!g_stopStatusChecker) {
        // Sleep in small 100ms intervals to check g_stopStatusChecker and shutdown quickly
        for (int i = 0; i < 50 && !g_stopStatusChecker; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (g_stopStatusChecker) break;

        try {
            auto conn = Database::getInstance().getConnection();
            pqxx::work txn(*conn);
            
            // Find devices that are online but have no metrics in the last 30 seconds
            pqxx::result res = txn.exec_prepared("find_timed_out_devices");
            
            std::vector<std::tuple<int, std::string, std::string>> timedOutDevices;
            for (auto const &row : res) {
                timedOutDevices.push_back({
                    row["id"].as<int>(),
                    row["name"].as<std::string>(),
                    row["ip_address"].as<std::string>()
                });
            }

            if (!timedOutDevices.empty()) {
                for (const auto& [id, name, ip] : timedOutDevices) {
                    std::cout << "[StatusChecker] Device '" << name << "' (ID: " << id << ") timed out. Marking offline." << std::endl;
                    txn.exec_prepared("update_device_status", "offline", id);
                           
                    // Create offline event JSON
                    nlohmann::json eventJson = {
                        {"device_id", id},
                        {"device_name", name},
                        {"ip_address", ip},
                        {"timestamp", EventBroker::getCurrentTimestamp()}
                    };
                    
                    EventBroker::getInstance().publish("device_offline", eventJson.dump());
                }
                txn.commit();
            }
        } catch (const std::exception& e) {
            std::cerr << "[StatusChecker] Error checking device statuses: " << e.what() << std::endl;
        }
    }
    std::cout << "[StatusChecker] Background service stopped." << std::endl;
}

int main() {
    try {
        std::cout << "[Server] Loading configuration..." << std::endl;
        Config::getInstance().load("config.json");

        std::cout << "[Server] Initializing database connection pool..." << std::endl;
        Database::getInstance().initialize();

        std::cout << "[Server] Starting Crow web application..." << std::endl;
        
        // Define Crow App with CORSMiddleware (global) and AuthMiddleware (local)
        crow::App<CORSMiddleware, AuthMiddleware> app;


        // Register routes for controllers
        AuthController::registerRoutes(app);
        DeviceController::registerRoutes(app);
        MetricController::registerRoutes(app);
        AlertController::registerRoutes(app);

        // Health check endpoint for Railway / uptime monitoring (no auth required)
        CROW_ROUTE(app, "/api/health")
        .methods(crow::HTTPMethod::GET)
        ([]() {
            crow::response res;
            res.set_header("Content-Type", "application/json");
            try {
                auto conn = Database::getInstance().getConnection();
                pqxx::nontransaction txn(*conn);
                txn.exec("SELECT 1");
                res.code = 200;
                res.write(R"({"status":"ok","database":"connected"})");
            } catch (const std::exception& e) {
                res.code = 503;
                res.write(R"({"status":"degraded","database":"unreachable"})");
            }
            return res;
        });

        // Explicit CORS preflight OPTIONS handlers for all API routes
        CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/signup").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/register-device").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/devices").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/device/<int>").methods(crow::HTTPMethod::OPTIONS)([](int id) { return crow::response(204); });
        CROW_ROUTE(app, "/api/device/<int>/metrics").methods(crow::HTTPMethod::OPTIONS)([](int id) { return crow::response(204); });
        CROW_ROUTE(app, "/api/alerts").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/events").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/metrics").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/deploy/agent").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/device-users").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });
        CROW_ROUTE(app, "/api/device/<int>/assign").methods(crow::HTTPMethod::OPTIONS)([](int id) { return crow::response(204); });
        CROW_ROUTE(app, "/api/metrics/trends").methods(crow::HTTPMethod::OPTIONS)([]() { return crow::response(204); });

        // Get server configuration
        auto& config = Config::getInstance();
        std::cout << "[Server] Running on http://" << config.serverHost << ":" << config.serverPort << std::endl;

        // Start background status checker
        g_stopStatusChecker = false;
        g_statusCheckerThread = std::thread(runStatusChecker);

        // Start the server
        int port = config.serverPort;
        if (const char* railwayPort = std::getenv("PORT")) {
            try {
                port = std::stoi(railwayPort);
                std::cout << "[Server] Railway PORT detected: " << port << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[Server] Invalid PORT env var value: " << railwayPort << ", error: " << e.what() << std::endl;
            }
        }

        app.port(port)
           .bindaddr("0.0.0.0")
           .multithreaded()
           .run();

        // Stop background status checker
        std::cout << "[Server] Shutting down background services..." << std::endl;
        g_stopStatusChecker = true;
        if (g_statusCheckerThread.joinable()) {
            g_statusCheckerThread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] Server failed to start: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
