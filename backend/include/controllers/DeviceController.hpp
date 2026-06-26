#ifndef DEVICE_CONTROLLER_HPP
#define DEVICE_CONTROLLER_HPP

#include <crow.h>
#include "middleware/AuthMiddleware.hpp"
#include "middleware/CORSMiddleware.hpp"

class DeviceController {
public:
    /**
     * Registers device-related routes on the Crow application.
     */
    static void registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app);
};


#endif // DEVICE_CONTROLLER_HPP
