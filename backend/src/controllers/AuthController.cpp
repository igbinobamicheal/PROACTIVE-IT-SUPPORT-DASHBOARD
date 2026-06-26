#include "controllers/AuthController.hpp"
#include "repositories/UserRepository.hpp"
#include "utils/BcryptUtil.hpp"
#include "utils/JWTUtil.hpp"
#include "config/Config.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

void AuthController::registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app) {
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("username") || !body.contains("password")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Username and password are required"}}.dump());
                return res;
            }

            std::string username = body["username"];
            std::string password = body["password"];

            UserRepository userRepo;
            auto userOpt = userRepo.findByUsername(username);

            if (!userOpt.has_value()) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Invalid username or password"}}.dump());
                return res;
            }

            User user = userOpt.value();
            if (!BcryptUtil::verifyPassword(password, user.passwordHash)) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Invalid username or password"}}.dump());
                return res;
            }

            auto& config = Config::getInstance();
            std::string token = JWTUtil::generateToken(username, config.jwtSecret, config.jwtExpirationHours);

            if (token.empty()) {
                res.code = 500;
                res.write(nlohmann::json{{"error", "Failed to generate token"}}.dump());
                return res;
            }

            res.code = 200;
            res.write(nlohmann::json{{"token", token}}.dump());
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
