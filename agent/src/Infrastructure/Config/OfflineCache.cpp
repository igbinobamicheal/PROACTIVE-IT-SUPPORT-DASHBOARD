#include "Infrastructure/Config/OfflineCache.h"
#include "Infrastructure/Logging/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace Infrastructure::Config {

// Custom JSON conversion helpers for Domain::Models::MetricData
static void to_json(nlohmann::json& j, const Domain::Models::MetricData& m) {
    j = nlohmann::json{
        {"cpu_usage", m.cpu_usage},
        {"ram_usage", m.ram_usage},
        {"disk_usage", m.disk_usage},
        {"network_in", m.network_in},
        {"network_out", m.network_out},
        {"uptime", m.uptime},
        {"hostname", m.hostname},
        {"username", m.username},
        {"windows_version", m.windows_version},
        {"mac_address", m.mac_address},
        {"local_ip", m.local_ip},
        {"machine_guid", m.machine_guid}
    };
}

static void from_json(const nlohmann::json& j, Domain::Models::MetricData& m) {
    j.at("cpu_usage").get_to(m.cpu_usage);
    j.at("ram_usage").get_to(m.ram_usage);
    j.at("disk_usage").get_to(m.disk_usage);
    j.at("network_in").get_to(m.network_in);
    j.at("network_out").get_to(m.network_out);
    j.at("uptime").get_to(m.uptime);
    j.at("hostname").get_to(m.hostname);
    j.at("username").get_to(m.username);
    j.at("windows_version").get_to(m.windows_version);
    j.at("mac_address").get_to(m.mac_address);
    j.at("local_ip").get_to(m.local_ip);
    j.at("machine_guid").get_to(m.machine_guid);
}

OfflineCache::OfflineCache(const std::string& cacheFilePath)
    : cacheFilePath_(cacheFilePath) {
    Load();
}

void OfflineCache::Push(const Domain::Models::MetricData& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Add metrics
    cache_.push_back(metrics);

    // Limit cache to 1000 items (FIFO eviction)
    if (cache_.size() > 1000) {
        cache_.erase(cache_.begin());
    }

    Save();
}

std::vector<Domain::Models::MetricData> OfflineCache::PopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Domain::Models::MetricData> copy = cache_;
    cache_.clear();
    Save();
    return copy;
}

size_t OfflineCache::Size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

void OfflineCache::Load() {
    using namespace Logging;

    if (!std::filesystem::exists(cacheFilePath_)) {
        return; // Cache file does not exist yet; start clean
    }

    try {
        std::ifstream file(cacheFilePath_);
        if (!file.is_open()) {
            Logger::GetInstance().Warning("OfflineCache: Failed to open cache file for reading: " + cacheFilePath_);
            return;
        }

        nlohmann::json j;
        file >> j;
        file.close();

        if (j.is_array()) {
            cache_.clear();
            for (const auto& item : j) {
                Domain::Models::MetricData data;
                from_json(item, data);
                cache_.push_back(data);
            }
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("OfflineCache: Exception while loading cache file: " + std::string(e.what()));
    }
}

void OfflineCache::Save() {
    using namespace Logging;

    try {
        // Ensure parent directory exists
        std::filesystem::path p(cacheFilePath_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json jArr = nlohmann::json::array();
        for (const auto& metrics : cache_) {
            nlohmann::json j;
            to_json(j, metrics);
            jArr.push_back(j);
        }

        std::ofstream file(cacheFilePath_, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            Logger::GetInstance().Error("OfflineCache: Failed to open cache file for writing: " + cacheFilePath_);
            return;
        }

        file << jArr.dump();
        file.close();
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("OfflineCache: Exception while saving cache file: " + std::string(e.what()));
    }
}

} // namespace Infrastructure::Config
