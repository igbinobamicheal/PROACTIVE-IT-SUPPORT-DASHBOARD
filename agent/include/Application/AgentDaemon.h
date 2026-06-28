#pragma once

#include "Domain/RepositoryInterfaces/IConfigRepository.h"
#include "Domain/ServiceInterfaces/IMetricCollector.h"
#include "Domain/ServiceInterfaces/IHttpSender.h"
#include "Infrastructure/Config/OfflineCache.h"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

namespace Application {

class AgentDaemon {
public:
    AgentDaemon(std::unique_ptr<Domain::RepositoryInterfaces::IConfigRepository> configRepo,
                std::unique_ptr<Domain::ServiceInterfaces::IMetricCollector> metricCollector,
                std::unique_ptr<Domain::ServiceInterfaces::IHttpSender> httpSender,
                const std::string& cacheFilePath);
    ~AgentDaemon();

    bool Start();
    void Stop();
    bool IsRunning() const;

private:
    void MetricsLoop();
    void HeartbeatLoop();
    void ReconnectLoop();
    void UpdateLoop();

    bool ValidateConfig(const Domain::Models::AgentConfig& config);
    void TriggerReconnect();
    void FlushOfflineCache(const Domain::Models::AgentConfig& config);
    bool IsNewerVersion(const std::string& current, const std::string& latest);
    std::string CalculateFileSHA256(const std::string& filePath);
    void ExecuteUpdaterScript();

    std::unique_ptr<Domain::RepositoryInterfaces::IConfigRepository> configRepo_;
    std::unique_ptr<Domain::ServiceInterfaces::IMetricCollector> metricCollector_;
    std::unique_ptr<Domain::ServiceInterfaces::IHttpSender> httpSender_;
    std::unique_ptr<Infrastructure::Config::OfflineCache> offlineCache_;

    std::thread metricsThread_;
    std::thread heartbeatThread_;
    std::thread reconnectThread_;
    std::thread updateThread_;

    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    
    std::atomic<int> backoffSeconds_;
    std::atomic<int> consecutiveFailures_;
    std::atomic<bool> reconnectTriggered_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable cvReconnect_;
};

} // namespace Application
