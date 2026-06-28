#pragma once

#include <string>

namespace Domain::Models {

struct AgentConfig {
    std::string server_url;
    int device_id = 0;
    std::string agent_secret;
};

} // namespace Domain::Models
