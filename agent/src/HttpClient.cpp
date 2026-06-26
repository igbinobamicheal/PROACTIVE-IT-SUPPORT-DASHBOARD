#include "HttpClient.h"
#include <cpr/cpr.h>
#include <iostream>

int HttpClient::post(const std::string& url, const std::string& jsonPayload, const std::string& token, std::string& responseOut) {
    cpr::Header headers{{"Content-Type", "application/json"}};
    
    if (!token.empty()) {
        // Determine the type of token based on the request URL
        if (url.find("/api/register-device") != std::string::npos) {
            headers.insert({"Authorization", "Bearer " + token});
        } else {
            headers.insert({"X-Device-Token", token});
        }
    }
    
    try {
        cpr::Response r = cpr::Post(cpr::Url{url},
                                    cpr::Header{headers},
                                    cpr::Body{jsonPayload});
        
        responseOut = r.text;
        return static_cast<int>(r.status_code);
    } catch (const std::exception& e) {
        std::cerr << "[HttpClient] Network error during POST to " << url << ": " << e.what() << std::endl;
        return -1;
    }
}
