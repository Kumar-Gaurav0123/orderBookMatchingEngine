#include "Logging.hpp"
#include <chrono>
#include <ctime>

std::atomic<LogLevel> Logger::globalLevel_ = LogLevel::INFO;

void Logger::setLevel(LogLevel lvl) {
    globalLevel_.store(lvl);
}

static std::string ts() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));

    return {buf};
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < static_cast<int>(globalLevel_.load())) return;

    std::ostream& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;

    out << "[" << ts() << "] ";

    switch (level) {
        case LogLevel::TRACE: out << "[TRACE] "; break;
        case LogLevel::DEBUG: out << "[DEBUG] "; break;
        case LogLevel::INFO : out << "[INFO ] "; break;
        case LogLevel::WARN : out << "[WARN ] "; break;
        case LogLevel::ERROR: out << "[ERROR] "; break;
        default: break;
    }

    out << msg << "\n";
}

void Logger::trace(const std::string& msg) { log(LogLevel::TRACE, msg); }
void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info (const std::string& msg) { log(LogLevel::INFO , msg); }
void Logger::warn (const std::string& msg) { log(LogLevel::WARN , msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }
