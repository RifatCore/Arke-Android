#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iostream>

#if defined(__ANDROID__)
#include <android/log.h>
#define ARKE_LOG_TAG "Arke-Android"
#endif

namespace arke {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& logDir);
    void log(LogLevel level, std::string_view msg);

    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void info(std::string_view msg)  { log(LogLevel::Info, msg); }
    void warn(std::string_view msg)  { log(LogLevel::Warn, msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }

private:
    Logger() = default;
    ~Logger();

    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_initialized{false};
};

} // namespace arke
