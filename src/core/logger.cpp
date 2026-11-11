#include "arke/logger.hpp"
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace arke {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

static void posix_mkdir_recursive(const std::string& path) {
    if (path.empty()) return;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            if (current != "/" && !current.empty()) {
                struct stat st;
                if (stat(current.c_str(), &st) != 0) {
                    mkdir(current.c_str(), 0755);
                }
            }
        }
    }
}

void Logger::init(const std::string& logDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile) {
        fclose(m_logFile);
        m_logFile = nullptr;
    }

    posix_mkdir_recursive(logDir);
    std::string logPath = logDir + "/arke.log";
    m_logFile = fopen(logPath.c_str(), "a");
    m_initialized = (m_logFile != nullptr);

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, ARKE_LOG_TAG,
                        "Logger initialized: %s (file status: %s)",
                        logPath.c_str(), m_initialized ? "OPEN" : "UNAVAILABLE");
#endif
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile) {
        fflush(m_logFile);
        fclose(m_logFile);
        m_logFile = nullptr;
    }
}

void Logger::log(LogLevel level, std::string_view msg) {
    const char* levelStr = "INFO";
    int androidPriority = ANDROID_LOG_INFO;
    switch (level) {
        case LogLevel::Debug: levelStr = "DEBUG"; androidPriority = ANDROID_LOG_DEBUG; break;
        case LogLevel::Info:  levelStr = "INFO "; androidPriority = ANDROID_LOG_INFO; break;
        case LogLevel::Warn:  levelStr = "WARN "; androidPriority = ANDROID_LOG_WARN; break;
        case LogLevel::Error: levelStr = "ERROR"; androidPriority = ANDROID_LOG_ERROR; break;
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S") << "] ["
       << levelStr << "] " << msg;
    std::string formatted = ss.str();

#if defined(__ANDROID__)
    __android_log_print(androidPriority, ARKE_LOG_TAG, "%s", formatted.c_str());
#else
    std::cout << formatted << std::endl;
#endif

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized && m_logFile) {
        fputs(formatted.c_str(), m_logFile);
        fputc('\n', m_logFile);
        fflush(m_logFile);
    }
}

} // namespace arke
