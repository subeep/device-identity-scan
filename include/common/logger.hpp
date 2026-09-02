#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <deque>
#include <vector>

namespace discan {

enum class LogLevel {
    DEBUG_LVL = 0,
    INFO_LVL,
    WARN_LVL,
    ERROR_LVL
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void log(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        
        LogEntry entry{now, level, msg};
        entries_.push_back(entry);
        if (entries_.size() > max_entries_) {
            entries_.pop_front();
        }

        // Also print to stdout/stderr
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        
        std::ostream& out = (level == LogLevel::ERROR_LVL) ? std::cerr : std::cout;
        out << "[" << std::put_time(std::localtime(&in_time_t), "%H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ["
            << level_str(level) << "] " << msg << "\n";
    }

    std::vector<LogEntry> get_entries() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<LogEntry>(entries_.begin(), entries_.end());
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    static const char* level_str(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG_LVL: return "DEBUG";
            case LogLevel::INFO_LVL:  return "INFO ";
            case LogLevel::WARN_LVL:  return "WARN ";
            case LogLevel::ERROR_LVL: return "ERROR";
            default: return "LOG  ";
        }
    }

private:
    Logger() = default;
    std::mutex mutex_;
    std::deque<LogEntry> entries_;
    size_t max_entries_ = 1000;
};

#define DISCAN_LOG_DEBUG(msg) do { std::ostringstream ss; ss << msg; discan::Logger::instance().log(discan::LogLevel::DEBUG_LVL, ss.str()); } while(0)
#define DISCAN_LOG_INFO(msg)  do { std::ostringstream ss; ss << msg; discan::Logger::instance().log(discan::LogLevel::INFO_LVL,  ss.str()); } while(0)
#define DISCAN_LOG_WARN(msg)  do { std::ostringstream ss; ss << msg; discan::Logger::instance().log(discan::LogLevel::WARN_LVL,  ss.str()); } while(0)
#define DISCAN_LOG_ERROR(msg) do { std::ostringstream ss; ss << msg; discan::Logger::instance().log(discan::LogLevel::ERROR_LVL, ss.str()); } while(0)

} // namespace discan
