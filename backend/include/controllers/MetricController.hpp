#ifndef METRIC_CONTROLLER_HPP
#define METRIC_CONTROLLER_HPP

#include <crow.h>
#include "middleware/AuthMiddleware.hpp"
#include "middleware/CORSMiddleware.hpp"

class MetricController {
public:
    /**
     * Registers metric-related routes on the Crow application.
     */
    static void registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app);
};


#endif // METRIC_CONTROLLER_HPP
