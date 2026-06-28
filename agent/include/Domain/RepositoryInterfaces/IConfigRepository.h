#pragma once

#include "Domain/Models/AgentConfig.h"

namespace Domain::RepositoryInterfaces {

class IConfigRepository {
public:
    virtual ~IConfigRepository() = default;
    virtual bool Load(Models::AgentConfig& config) = 0;
    virtual bool Save(const Models::AgentConfig& config) = 0;
};

} // namespace Domain::RepositoryInterfaces
