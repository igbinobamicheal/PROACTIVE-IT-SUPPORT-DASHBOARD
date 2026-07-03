#pragma once

#include "Domain/ServiceInterfaces/IHttpSender.h"
#include <string>

namespace Infrastructure::Network {

class CprHttpSender : public Domain::ServiceInterfaces::IHttpSender {
public:
    CprHttpSender() = default;

    bool SendMetrics(const std::string& serverUrl, const std::string& token, const Domain::Models::MetricData& metrics) override;
    bool SendDiagnostics(const std::string& serverUrl, const std::string& token, const std::string& diagnosticsJson) override;
    bool RegisterDevice(const std::string& serverUrl, 
                        const Domain::Models::MetricData& metrics,
                        const std::string& registrationToken, 
                        int& deviceId, 
                        std::string& agentSecret) override;
    bool SendHeartbeat(const std::string& serverUrl, const std::string& token) override;
    bool GetLatestAgentInfo(const std::string& serverUrl, 
                            std::string& latestVersion, 
                            std::string& downloadUrl, 
                            std::string& checksum) override;
    bool DownloadFile(const std::string& downloadUrl, const std::string& destinationPath) override;

private:
    bool ValidateHttps(const std::string& url);
    std::string BuildEndpointUrl(const std::string& serverUrl, const std::string& path);
};

} // namespace Infrastructure::Network
