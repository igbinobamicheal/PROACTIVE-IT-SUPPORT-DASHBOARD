#pragma once

#include "Domain/Models/MetricData.h"
#include <string>

namespace Domain::ServiceInterfaces {

class IHttpSender {
public:
    virtual ~IHttpSender() = default;
    virtual bool SendMetrics(const std::string& serverUrl, const std::string& token, const Models::MetricData& metrics) = 0;
    virtual bool SendDiagnostics(const std::string& serverUrl, const std::string& token, const std::string& diagnosticsJson) = 0;
    virtual bool RegisterDevice(const std::string& serverUrl, 
                                const Models::MetricData& metrics,
                                const std::string& registrationToken, 
                                int& deviceId, 
                                std::string& agentSecret) = 0;
    virtual bool SendHeartbeat(const std::string& serverUrl, const std::string& token) = 0;
    virtual bool GetLatestAgentInfo(const std::string& serverUrl, 
                                    std::string& latestVersion, 
                                    std::string& downloadUrl, 
                                    std::string& checksum) = 0;
    virtual bool DownloadFile(const std::string& downloadUrl, const std::string& destinationPath) = 0;
};

} // namespace Domain::ServiceInterfaces
