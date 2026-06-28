#pragma once

#include <string>
#include <mutex>
#include <fstream>

namespace Infrastructure::Logging {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& GetInstance();

    void Initialize(const std::string& logFilePath);
    void Log(LogLevel level, const std::string& message);

    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string GetTimestamp();
    std::string LevelToString(LogLevel level);

    std::mutex mutex_;
    std::ofstream logFile_;
    std::string logFilePath_;
};

} // namespace Infrastructure::Logging
