#ifndef ALERT_CONTROLLER_HPP
#define ALERT_CONTROLLER_HPP

#include <crow.h>
#include "middleware/AuthMiddleware.hpp"
#include "middleware/CORSMiddleware.hpp"

class AlertController {
public:
    /**
     * Registers alert-related routes on the Crow application.
     */
    static void registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app);
};

#endif // ALERT_CONTROLLER_HPP
