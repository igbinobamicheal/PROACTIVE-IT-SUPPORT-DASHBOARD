#ifndef AUTH_MIDDLEWARE_HPP
#define AUTH_MIDDLEWARE_HPP

#include <crow.h>
#include "utils/JWTUtil.hpp"
#include "config/Config.hpp"
#include <nlohmann/json.hpp>
#include <string>

struct AuthMiddleware : crow::ILocalMiddleware {
    struct context {
        std::string username;
    };

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        std::string token;
        
        // 1. Try to find in Authorization header
        std::string authHeader = req.get_header_value("Authorization");
        if (!authHeader.empty()) {
            if (authHeader.find("Bearer ") == 0) {
                token = authHeader.substr(7); // "Bearer " is 7 chars long
            } else {
                res.code = 401;
                res.set_header("Content-Type", "application/json");
                res.write(nlohmann::json{{"error", "Invalid Authorization header format. Must be Bearer <token>"}}.dump());
                res.end();
                return;
            }
        } else {
            // 2. Try query parameter (fallback for SSE EventSource)
            auto tokenParam = req.url_params.get("token");
            if (tokenParam) {
                token = tokenParam;
            }
        }

        if (token.empty()) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(nlohmann::json{{"error", "Authorization token is missing"}}.dump());
            res.end();
            return;
        }
        auto& config = Config::getInstance();
        std::string username = JWTUtil::verifyToken(token, config.jwtSecret);

        if (username.empty()) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(nlohmann::json{{"error", "Invalid or expired authentication token"}}.dump());
            res.end();
            return;
        }

        ctx.username = username;
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        // No action needed after handling
    }
};

#endif // AUTH_MIDDLEWARE_HPP
