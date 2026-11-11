#include "arke/logger.hpp"
#include <chrono>
#include <iomanip>
#include <filesystem>

namespace arke {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::init(const std::string& logDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    try {
        std::error_code ec;
        std::filesystem::create_directories(logDir, ec);
        std::string logPath = logDir + "/arke.log";
        m_logFile.open(logPath, std::ios::out | std::ios::app);
        m_initialized = m_logFile.is_open();
    } catch (...) {
        m_initialized = false;
    }
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

void Logger::log(LogLevel level, std::string_view msg) {
    const char* levelStr = "INFO";
    int androidPriority = 4; // ANDROID_LOG_INFO
    switch (level) {
        case LogLevel::Debug: levelStr = "DEBUG"; androidPriority = 3; break;
        case LogLevel::Info:  levelStr = "INFO "; androidPriority = 4; break;
        case LogLevel::Warn:  levelStr = "WARN "; androidPriority = 5; break;
        case LogLevel::Error: levelStr = "ERROR"; androidPriority = 6; break;
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
    if (m_initialized && m_logFile.is_open()) {
        m_logFile << formatted << "\n";
        m_logFile.flush();
    }
}

} // namespace arke
