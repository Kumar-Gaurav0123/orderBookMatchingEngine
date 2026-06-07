#pragma once

#include <string>
#include <iostream>
#include <atomic>

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, NONE };

class Logger {
public:
    static void setLevel(LogLevel level);

    static void log(LogLevel level, const std::string& msg);

    static void trace(const std::string& msg);
    static void debug(const std::string& msg);
    static void info (const std::string& msg);
    static void warn (const std::string& msg);
    static void error(const std::string& msg);

private:
    static std::atomic<LogLevel> globalLevel_;
};
