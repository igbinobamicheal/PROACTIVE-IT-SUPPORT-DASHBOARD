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
            auto session = Database::getInstance().getSession();
            
            // Find devices that are online but have no metrics in the last 30 seconds
            auto result = session.sql("SELECT id, name, ip_address FROM devices "
                                       "WHERE status = 'online' AND id NOT IN ("
                                       "  SELECT DISTINCT device_id FROM metrics "
                                       "  WHERE timestamp >= DATE_SUB(NOW(), INTERVAL 30 SECOND)"
                                       ")").execute();
            
            std::vector<std::tuple<int, std::string, std::string>> timedOutDevices;
            while (auto row = result.fetchOne()) {
                timedOutDevices.push_back({
                    row[0].get<int>(),
                    row[1].get<std::string>(),
                    row[2].get<std::string>()
                });
            }

            if (!timedOutDevices.empty()) {
                session.startTransaction();
                for (const auto& [id, name, ip] : timedOutDevices) {
                    std::cout << "[StatusChecker] Device '" << name << "' (ID: " << id << ") timed out. Marking offline." << std::endl;
                    session.sql("UPDATE devices SET status = 'offline' WHERE id = ?")
                           .bind(id)
                           .execute();
                           
                    // Create offline event JSON
                    nlohmann::json eventJson = {
                        {"device_id", id},
                        {"device_name", name},
                        {"ip_address", ip},
                        {"timestamp", EventBroker::getCurrentTimestamp()}
                    };
                    
                    EventBroker::getInstance().publish("device_offline", eventJson.dump());
                }
                session.commit();
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

        // Get server configuration
        auto& config = Config::getInstance();
        std::cout << "[Server] Running on http://" << config.serverHost << ":" << config.serverPort << std::endl;

        // Start background status checker
        g_stopStatusChecker = false;
        g_statusCheckerThread = std::thread(runStatusChecker);

        // Start the server
        app.port(config.serverPort)
           .bindaddr(config.serverHost)
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
