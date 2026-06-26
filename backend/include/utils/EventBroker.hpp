#ifndef EVENT_BROKER_HPP
#define EVENT_BROKER_HPP

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

struct Event {
    std::string type;
    std::string data;
};

class EventBroker {
public:
    /**
     * Gets the singleton instance of EventBroker.
     */
    static EventBroker& getInstance();

    /**
     * Publishes an event to all currently waiting client queues.
     * @param eventType The type of the event (e.g., "metric", "device_online").
     * @param data The JSON payload string.
     */
    void publish(const std::string& eventType, const std::string& data);

    /**
     * Waits for an event to be published, or times out.
     * @param queue A shared queue instance allocated per-request.
     * @param outEvent Reference to store the received event data.
     * @param timeoutMs The wait timeout in milliseconds.
     * @return true if an event was received, false if it timed out.
     */
    bool waitForEvent(std::shared_ptr<std::queue<Event>> queue, Event& outEvent, int timeoutMs = 15000);

    /**
     * Formats and returns the current time as a YYYY-MM-DD HH:MM:SS string.
     */
    static std::string getCurrentTimestamp();

private:
    EventBroker() = default;
    ~EventBroker() = default;
    EventBroker(const EventBroker&) = delete;
    EventBroker& operator=(const EventBroker&) = delete;

    void removeQueue(std::shared_ptr<std::queue<Event>> queue);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<std::queue<Event>>> clientQueues_;
};

#endif // EVENT_BROKER_HPP
