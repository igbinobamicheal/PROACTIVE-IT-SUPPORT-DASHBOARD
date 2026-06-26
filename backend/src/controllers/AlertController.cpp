#include "controllers/AlertController.hpp"
#include "repositories/AlertRepository.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

void AlertController::registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app) {
    // GET /api/alerts (guarded by JWT)
    CROW_ROUTE(app, "/api/alerts")
    .methods(crow::HTTPMethod::GET)
    .CROW_MIDDLEWARES(app, AuthMiddleware)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            AlertRepository alertRepo;
            std::vector<Alert> alerts;
            
            // Check if active filter query param is true
            auto activeParam = req.url_params.get("active");
            bool activeOnly = (activeParam != nullptr && std::string(activeParam) == "true");

            if (activeOnly) {
                alerts = alertRepo.findActive();
            } else {
                alerts = alertRepo.findAll(50); // Get last 50 alerts by default
            }

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& a : alerts) {
                arr.push_back(a);
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
}
