#pragma once

#include "Domain/Models/MetricData.h"
#include <vector>
#include <mutex>
#include <string>

namespace Infrastructure::Config {

class OfflineCache {
public:
    explicit OfflineCache(const std::string& cacheFilePath);
    ~OfflineCache() = default;

    void Push(const Domain::Models::MetricData& metrics);
    std::vector<Domain::Models::MetricData> PopAll();
    size_t Size();

private:
    void Load();
    void Save();

    std::string cacheFilePath_;
    std::vector<Domain::Models::MetricData> cache_;
    std::mutex mutex_;
};

} // namespace Infrastructure::Config
