#include "utils/EventBroker.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

EventBroker& EventBroker::getInstance() {
    static EventBroker instance;
    return instance;
}

void EventBroker::publish(const std::string& eventType, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Event event{eventType, data};
    for (const auto& q : clientQueues_) {
        q->push(event);
    }
    
    cv_.notify_all();
}

bool EventBroker::waitForEvent(std::shared_ptr<std::queue<Event>> queue, Event& outEvent, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Register client queue
    if (std::find(clientQueues_.begin(), clientQueues_.end(), queue) == clientQueues_.end()) {
        clientQueues_.push_back(queue);
    }
    
    // Wait until queue contains events or times out
    bool hasEvent = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() {
        return !queue->empty();
    });
    
    if (hasEvent) {
        outEvent = queue->front();
        queue->pop();
        removeQueue(queue);
        return true;
    }
    
    removeQueue(queue);
    return false;
}

void EventBroker::removeQueue(std::shared_ptr<std::queue<Event>> queue) {
    auto it = std::find(clientQueues_.begin(), clientQueues_.end(), queue);
    if (it != clientQueues_.end()) {
        clientQueues_.erase(it);
    }
}

std::string EventBroker::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
