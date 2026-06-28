#include "Infrastructure/Logging/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace Infrastructure::Logging {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    logFilePath_ = logFilePath;
    
    if (logFile_.is_open()) {
        logFile_.close();
    }

    try {
        // Ensure path exists
        std::filesystem::path p(logFilePath_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        
        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        if (!logFile_) {
            std::cerr << "[Logger Error] Failed to open log file: " << logFilePath_ << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Logger Error] Exception during initialization: " << e.what() << std::endl;
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    
    std::stringstream ss;
    ss << "[" << timestamp << "] [" << levelStr << "] " << message << "\n";
    std::string logMsg = ss.str();

    // Output to stdout/stderr
    if (level == LogLevel::Error) {
        std::cerr << logMsg << std::flush;
    } else {
        std::cout << logMsg << std::flush;
    }

    // Check for log rotation if a file path is defined
    if (!logFilePath_.empty()) {
        try {
            std::filesystem::path p(logFilePath_);
            if (std::filesystem::exists(p) && std::filesystem::file_size(p) >= 5 * 1024 * 1024) { // 5 MB threshold
                if (logFile_.is_open()) {
                    logFile_.close();
                }
                
                std::string path3 = logFilePath_ + ".3";
                std::string path2 = logFilePath_ + ".2";
                std::string path1 = logFilePath_ + ".1";
                
                if (std::filesystem::exists(path3)) {
                    std::filesystem::remove(path3);
                }
                if (std::filesystem::exists(path2)) {
                    std::filesystem::rename(path2, path3);
                }
                if (std::filesystem::exists(path1)) {
                    std::filesystem::rename(path1, path2);
                }
                std::filesystem::rename(logFilePath_, path1);
                
                // Re-open log file in truncate mode
                logFile_.open(logFilePath_, std::ios::out | std::ios::trunc);
            }
        } catch (...) {
            // Safe fallback
        }
    }

    // Output to file
    if (logFile_.is_open()) {
        logFile_ << logMsg << std::flush;
    } else if (!logFilePath_.empty()) {
        // Try to re-open if it wasn't open
        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        if (logFile_) {
            logFile_ << logMsg << std::flush;
        }
    }
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::Error, message);
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    // Sub-second precision
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::tm tm_now;
#if defined(_MSC_VER)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::stringstream ss;
    ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
        default:                return "DEBUG";
    }
}

} // namespace Infrastructure::Logging
