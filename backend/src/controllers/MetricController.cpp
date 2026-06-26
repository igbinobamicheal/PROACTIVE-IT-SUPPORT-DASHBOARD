#include "controllers/MetricController.hpp"
#include "repositories/DeviceRepository.hpp"
#include "repositories/MetricRepository.hpp"
#include "repositories/AlertRepository.hpp"
#include "utils/EventBroker.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

void MetricController::registerRoutes(crow::App<CORSMiddleware, AuthMiddleware>& app) {
    // POST /api/metrics (unguarded by user JWT, guarded by device token in X-Device-Token header)
    CROW_ROUTE(app, "/api/metrics")
    .methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        crow::response res;
        res.set_header("Content-Type", "application/json");

        // Extract device token from custom header "X-Device-Token"
        std::string deviceToken = req.get_header_value("X-Device-Token");
        if (deviceToken.empty()) {
            res.code = 401;
            res.write(nlohmann::json{{"error", "Missing X-Device-Token header"}}.dump());
            return res;
        }

        try {
            DeviceRepository devRepo;
            auto devOpt = devRepo.findByToken(deviceToken);
            if (!devOpt.has_value()) {
                res.code = 401;
                res.write(nlohmann::json{{"error", "Invalid device token"}}.dump());
                return res;
            }

            auto dev = devOpt.value();
            bool wentOnline = (dev.status == "offline");
            auto body = nlohmann::json::parse(req.body);
            
            // Check for required metric fields
            if (!body.contains("cpu_usage") || !body.contains("ram_usage") || 
                !body.contains("disk_usage") || !body.contains("network_in") || 
                !body.contains("network_out") || !body.contains("uptime")) {
                res.code = 400;
                res.write(nlohmann::json{{"error", "Missing one or more required metric fields"}}.dump());
                return res;
            }

            Metric metric;
            metric.deviceId = dev.id;
            metric.cpuUsage = body["cpu_usage"];
            metric.ramUsage = body["ram_usage"];
            metric.diskUsage = body["disk_usage"];
            metric.networkIn = body["network_in"];
            metric.networkOut = body["network_out"];
            metric.uptime = body["uptime"];

            MetricRepository metricRepo;
            metricRepo.create(metric);

            std::string ts = EventBroker::getCurrentTimestamp();

            // Publish online event if status changed
            if (wentOnline) {
                nlohmann::json onlineEvent = {
                    {"device_id", dev.id},
                    {"device_name", dev.name},
                    {"ip_address", dev.ipAddress},
                    {"timestamp", ts}
                };
                EventBroker::getInstance().publish("device_online", onlineEvent.dump());
            }

            // Publish new metric event
            nlohmann::json metricEvent = {
                {"device_id", dev.id},
                {"device_name", dev.name},
                {"cpu_usage", metric.cpuUsage},
                {"ram_usage", metric.ramUsage},
                {"disk_usage", metric.diskUsage},
                {"network_in", metric.networkIn},
                {"network_out", metric.networkOut},
                {"uptime", metric.uptime},
                {"timestamp", ts}
            };
            EventBroker::getInstance().publish("metric", metricEvent.dump());

            // --- Alert Engine Rules Evaluation ---
            AlertRepository alertRepo;

            // 1. CPU Threshold Alert (CPU > 90%)
            if (metric.cpuUsage > 90.0) {
                if (!alertRepo.hasActiveAlert(dev.id, "cpu")) {
                    Alert a;
                    a.deviceId = dev.id;
                    a.deviceName = dev.name;
                    a.ruleType = "cpu";
                    a.ruleValue = metric.cpuUsage;
                    a.threshold = 90.0;
                    a.message = "High CPU utilization: " + std::to_string(static_cast<int>(metric.cpuUsage)) + "% (Threshold: 90%)";
                    alertRepo.create(a);

                    nlohmann::json alertEvent = a;
                    EventBroker::getInstance().publish("new_alert", alertEvent.dump());
                }
            } else {
                int resolvedCount = alertRepo.resolveAlert(dev.id, "cpu");
                if (resolvedCount > 0) {
                    nlohmann::json resolveEvent = {
                        {"device_id", dev.id},
                        {"device_name", dev.name},
                        {"rule_type", "cpu"},
                        {"timestamp", ts}
                    };
                    EventBroker::getInstance().publish("alert_resolved", resolveEvent.dump());
                }
            }

            // 2. RAM Threshold Alert (RAM > 90%)
            if (metric.ramUsage > 90.0) {
                if (!alertRepo.hasActiveAlert(dev.id, "ram")) {
                    Alert a;
                    a.deviceId = dev.id;
                    a.deviceName = dev.name;
                    a.ruleType = "ram";
                    a.ruleValue = metric.ramUsage;
                    a.threshold = 90.0;
                    a.message = "High RAM utilization: " + std::to_string(static_cast<int>(metric.ramUsage)) + "% (Threshold: 90%)";
                    alertRepo.create(a);

                    nlohmann::json alertEvent = a;
                    EventBroker::getInstance().publish("new_alert", alertEvent.dump());
                }
            } else {
                int resolvedCount = alertRepo.resolveAlert(dev.id, "ram");
                if (resolvedCount > 0) {
                    nlohmann::json resolveEvent = {
                        {"device_id", dev.id},
                        {"device_name", dev.name},
                        {"rule_type", "ram"},
                        {"timestamp", ts}
                    };
                    EventBroker::getInstance().publish("alert_resolved", resolveEvent.dump());
                }
            }

            // 3. Disk Threshold Alert (Disk > 95%)
            if (metric.diskUsage > 95.0) {
                if (!alertRepo.hasActiveAlert(dev.id, "disk")) {
                    Alert a;
                    a.deviceId = dev.id;
                    a.deviceName = dev.name;
                    a.ruleType = "disk";
                    a.ruleValue = metric.diskUsage;
                    a.threshold = 95.0;
                    a.message = "High Disk space utilization: " + std::to_string(static_cast<int>(metric.diskUsage)) + "% (Threshold: 95%)";
                    alertRepo.create(a);

                    nlohmann::json alertEvent = a;
                    EventBroker::getInstance().publish("new_alert", alertEvent.dump());
                }
            } else {
                int resolvedCount = alertRepo.resolveAlert(dev.id, "disk");
                if (resolvedCount > 0) {
                    nlohmann::json resolveEvent = {
                        {"device_id", dev.id},
                        {"device_name", dev.name},
                        {"rule_type", "disk"},
                        {"timestamp", ts}
                    };
                    EventBroker::getInstance().publish("alert_resolved", resolveEvent.dump());
                }
            }

            res.code = 201;
            res.write(nlohmann::json{{"message", "Metrics ingested successfully"}}.dump());
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
