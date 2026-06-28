#pragma once

#include "Domain/RepositoryInterfaces/IConfigRepository.h"
#include <string>

namespace Infrastructure::Config {

class JsonConfigRepository : public Domain::RepositoryInterfaces::IConfigRepository {
public:
    explicit JsonConfigRepository(const std::string& configFilePath);

    bool Load(Domain::Models::AgentConfig& config) override;
    bool Save(const Domain::Models::AgentConfig& config) override;

private:
    std::string configFilePath_;
};

} // namespace Infrastructure::Config
