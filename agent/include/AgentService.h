#ifndef AGENT_SERVICE_H
#define AGENT_SERVICE_H

#include <atomic>
#include <string>

class AgentService {
public:
    AgentService();
    
    /**
     * Starts the metric collection loop in the background.
     */
    void start(const std::string& configPath);

    /**
     * Stops the metric collection loop.
     */
    void stop();

private:
    std::atomic<bool> running;
    void runLoop(const std::string& configPath);
};

#endif // AGENT_SERVICE_H
