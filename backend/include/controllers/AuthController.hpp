#ifndef AUTH_CONTROLLER_HPP
#define AUTH_CONTROLLER_HPP

#include <crow.h>
#include "middleware/AuthMiddleware.hpp"
#include "middleware/CORSMiddleware.hpp"

class AuthController {
public:
    /**
     * Registers authentication routes (like POST /api/login) on the Crow application.
     */
    static void registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app);
};


#endif // AUTH_CONTROLLER_HPP
