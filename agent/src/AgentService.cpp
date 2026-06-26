#include "AgentService.h"
#include "ConfigManager.h"
#include "MetricCollector.h"
#include "HttpClient.h"
#include <thread>
#include <chrono>
#include <iostream>

AgentService::AgentService() : running(false) {}

void AgentService::start(const std::string& configPath) {
    if (running) return;
    running = true;
    std::thread(&AgentService::runLoop, this, configPath).detach();
    std::cout << "[AgentService] Service thread started." << std::endl;
}

void AgentService::stop() {
    running = false;
    std::cout << "[AgentService] Service thread stopping." << std::endl;
}

void AgentService::runLoop(const std::string& configPath) {
    MetricCollector collector;
    HttpClient client;
    
    while (running) {
        auto& config = ConfigManager::getInstance();
        
        // Ensure configuration is reloaded or active
        if (config.deviceToken.empty()) {
            std::cout << "[AgentService] Device is not registered. Waiting..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Collect metrics
        auto metricsJson = collector.collectAll();
        std::string payload = metricsJson.dump();
        std::string url = config.serverUrl + "/api/metrics";

        std::string response;
        int status = client.post(url, payload, config.deviceToken, response);
        
        if (status == 201) {
            std::cout << "[AgentService] Metrics sent successfully." << std::endl;
        } else {
            std::cerr << "[AgentService] Failed to send metrics. Status: " << status << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(config.intervalSeconds));
    }
}
