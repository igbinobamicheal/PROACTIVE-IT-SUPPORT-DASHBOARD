#include "Infrastructure/Network/CprHttpSender.h"
#include "Infrastructure/Logging/Logger.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>

namespace Infrastructure::Network {

bool CprHttpSender::ValidateHttps(const std::string& url) {
    std::string lowerUrl = url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (lowerUrl.rfind("https://", 0) == 0) {
        return true;
    }
    
    Logging::Logger::GetInstance().Error("Enforced HTTPS policy check failed for URL: " + url + " - Server URL must start with https://");
    return false;
}

std::string CprHttpSender::BuildEndpointUrl(const std::string& serverUrl, const std::string& path) {
    std::string url = serverUrl;
    // Remove trailing slash if present
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }

    // If serverUrl already contains "/api", and path starts with "/api"
    if (url.find("/api") != std::string::npos && path.rfind("/api", 0) == 0) {
        // Strip "/api" from path to avoid double nesting
        std::string strippedPath = path.substr(4);
        return url + strippedPath;
    }

    return url + path;
}

bool CprHttpSender::SendMetrics(const std::string& serverUrl, const std::string& token, const Domain::Models::MetricData& metrics) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(serverUrl)) {
        return false;
    }

    std::string endpoint = BuildEndpointUrl(serverUrl, "/api/metrics");

    try {
        nlohmann::json j;
        j["cpu_usage"] = metrics.cpu_usage;
        j["ram_usage"] = metrics.ram_usage;
        j["disk_usage"] = metrics.disk_usage;
        j["uptime"] = metrics.uptime;
        j["network_in"] = metrics.network_in;
        j["network_out"] = metrics.network_out;
        
        // Additional metadata (sent in payload for backend/future logging)
        j["hostname"] = metrics.hostname;
        j["username"] = metrics.username;
        j["windows_version"] = metrics.windows_version;
        j["mac_address"] = metrics.mac_address;
        j["local_ip"] = metrics.local_ip;
        j["machine_guid"] = metrics.machine_guid;

        std::string payload = j.dump();

        Logger::GetInstance().Info("Sending metrics payload to " + endpoint);

        auto response = cpr::Post(
            cpr::Url{endpoint},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"X-Device-Token", token},
                {"User-Agent", "ProactiveITAgent/2.0.0"}
            },
            cpr::Body{payload},
            cpr::VerifySsl{true},
            cpr::Timeout{5000} // 5 second timeout
        );

        if (response.status_code == 200 || response.status_code == 201) {
            Logger::GetInstance().Info("Metrics successfully sent to backend.");
            return true;
        } else {
            Logger::GetInstance().Error("Failed to send metrics. HTTP status: " + std::to_string(response.status_code) + ", Response: " + response.text);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while sending metrics: " + std::string(e.what()));
        return false;
    }
}

bool CprHttpSender::SendDiagnostics(const std::string& serverUrl, const std::string& token, const std::string& diagnosticsJson) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(serverUrl)) {
        return false;
    }

    std::string endpoint = BuildEndpointUrl(serverUrl, "/api/device/diagnostics");

    try {
        Logger::GetInstance().Info("Sending diagnostics payload to " + endpoint);

        auto response = cpr::Post(
            cpr::Url{endpoint},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"X-Device-Token", token},
                {"User-Agent", "ProactiveITAgent/2.0.0"}
            },
            cpr::Body{diagnosticsJson},
            cpr::VerifySsl{true},
            cpr::Timeout{15000} // 15s timeout
        );

        if (response.status_code == 200 || response.status_code == 201) {
            Logger::GetInstance().Info("Diagnostics successfully sent to backend.");
            return true;
        } else {
            Logger::GetInstance().Error("Failed to send diagnostics. HTTP status: " + std::to_string(response.status_code) + ", Response: " + response.text);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while sending diagnostics: " + std::string(e.what()));
        return false;
    }
}

bool CprHttpSender::RegisterDevice(const std::string& serverUrl, 
                                    const Domain::Models::MetricData& metrics,
                                    const std::string& registrationToken, 
                                    int& deviceId, 
                                    std::string& agentSecret) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(serverUrl)) {
        return false;
    }

    std::string endpoint = BuildEndpointUrl(serverUrl, "/api/register-device");

    try {
        nlohmann::json j;
        j["registration_token"] = registrationToken;
        j["name"] = metrics.hostname;
        j["machine_guid"] = metrics.machine_guid;
        j["mac_address"] = metrics.mac_address;
        j["windows_version"] = metrics.windows_version;
        j["ip_address"] = metrics.local_ip;

        std::string payload = j.dump();

        Logger::GetInstance().Info("Sending device registration request to " + endpoint);

        auto response = cpr::Post(
            cpr::Url{endpoint},
            cpr::Header{
                {"Content-Type", "application/json"},
                {"User-Agent", "ProactiveITAgent/2.0.0"}
            },
            cpr::Body{payload},
            cpr::VerifySsl{true},
            cpr::Timeout{10000} // 10 second timeout for registration
        );

        if (response.status_code == 200 || response.status_code == 201) {
            auto resJson = nlohmann::json::parse(response.text);
            if (resJson.contains("id") && resJson.contains("token")) {
                deviceId = resJson["id"].get<int>();
                agentSecret = resJson["token"].get<std::string>();
                Logger::GetInstance().Info("Device registered successfully. ID: " + std::to_string(deviceId));
                return true;
            } else {
                Logger::GetInstance().Error("Registration response was missing 'id' or 'token'. Response: " + response.text);
                return false;
            }
        } else {
            Logger::GetInstance().Error("Failed to register device. HTTP status: " + std::to_string(response.status_code) + ", Response: " + response.text);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while registering device: " + std::string(e.what()));
        return false;
    }
}

bool CprHttpSender::SendHeartbeat(const std::string& serverUrl, const std::string& token) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(serverUrl)) {
        return false;
    }

    std::string endpoint = BuildEndpointUrl(serverUrl, "/api/heartbeat");

    try {
        auto response = cpr::Post(
            cpr::Url{endpoint},
            cpr::Header{
                {"X-Device-Token", token},
                {"User-Agent", "ProactiveITAgent/2.0.0"}
            },
            cpr::VerifySsl{true},
            cpr::Timeout{3000} // 3 second timeout for heartbeats
        );

        if (response.status_code == 200 || response.status_code == 201) {
            return true;
        } else {
            Logger::GetInstance().Warning("Heartbeat failed. HTTP status: " + std::to_string(response.status_code));
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Warning("Exception while sending heartbeat: " + std::string(e.what()));
        return false;
    }
}

bool CprHttpSender::GetLatestAgentInfo(const std::string& serverUrl, 
                                        std::string& latestVersion, 
                                        std::string& downloadUrl, 
                                        std::string& checksum) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(serverUrl)) {
        return false;
    }

    std::string endpoint = BuildEndpointUrl(serverUrl, "/api/agent/latest");

    try {
        auto response = cpr::Get(
            cpr::Url{endpoint},
            cpr::Header{{"User-Agent", "ProactiveITAgent/2.0.0"}},
            cpr::VerifySsl{true},
            cpr::Timeout{5000} // 5 second timeout
        );

        if (response.status_code == 200) {
            auto j = nlohmann::json::parse(response.text);
            if (j.contains("latest_version") && j.contains("download_url") && j.contains("checksum")) {
                latestVersion = j["latest_version"].get<std::string>();
                downloadUrl = j["download_url"].get<std::string>();
                checksum = j["checksum"].get<std::string>();
                return true;
            }
            Logger::GetInstance().Error("Latest agent response was missing key fields. Response: " + response.text);
            return false;
        } else {
            Logger::GetInstance().Error("Failed to fetch latest agent version. HTTP status: " + std::to_string(response.status_code));
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("Exception while fetching latest agent info: " + std::string(e.what()));
        return false;
    }
}

bool CprHttpSender::DownloadFile(const std::string& downloadUrl, const std::string& destinationPath) {
    using namespace Infrastructure::Logging;

    if (!ValidateHttps(downloadUrl)) {
        return false;
    }

    try {
        std::ofstream ofs(destinationPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            Logger::GetInstance().Error("DownloadFile: Failed to open destination path for writing: " + destinationPath);
            return false;
        }

        Logger::GetInstance().Info("Downloading update file from: " + downloadUrl);

        auto response = cpr::Get(
            cpr::Url{downloadUrl},
            cpr::VerifySsl{true},
            cpr::WriteCallback{
                [](std::string_view data, intptr_t userdata) {
                    auto* fileStream = reinterpret_cast<std::ofstream*>(userdata);
                    fileStream->write(data.data(), data.size());
                    return true;
                },
                reinterpret_cast<intptr_t>(&ofs)
            },
            cpr::Timeout{120000} // 2 minute timeout for download
        );

        ofs.close();

        if (response.status_code == 200 || response.status_code == 201 || response.status_code == 307 || response.status_code == 302) {
            Logger::GetInstance().Info("DownloadFile: Download completed successfully.");
            return true;
        } else {
            Logger::GetInstance().Error("DownloadFile: HTTP error during download. Status: " + std::to_string(response.status_code));
            std::filesystem::remove(destinationPath);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("DownloadFile: Exception during file download: " + std::string(e.what()));
        std::filesystem::remove(destinationPath);
        return false;
    }
}

} // namespace Infrastructure::Network
