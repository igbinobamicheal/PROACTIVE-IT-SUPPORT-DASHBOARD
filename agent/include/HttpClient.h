#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>

class HttpClient {
public:
    HttpClient() = default;

    /**
     * Sends a POST request with JSON body.
     * @param url The full request URL.
     * @param jsonPayload The JSON body string.
     * @param token Optional bearer or device token header value.
     * @param responseOut String to receive response body.
     * @return HTTP status code, or -1 on network failure.
     */
    int post(const std::string& url, const std::string& jsonPayload, const std::string& token, std::string& responseOut);
};

#endif // HTTP_CLIENT_H
