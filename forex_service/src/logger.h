/*
 * logger.h - Simple logging for LOB Forex Service
 */
#pragma once

#include <iostream>
#include <iomanip>
#include <ctime>
#include <string>

inline std::string timestamp() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

#define LOG(level, msg) \
    std::cout << "[" << timestamp() << "] [" << level << "] " << msg << std::endl

#define LOG_INFO(msg)  LOG("INFO", msg)
#define LOG_WARN(msg)  LOG("WARN", msg)
#define LOG_ERROR(msg) LOG("ERROR", msg)
#define LOG_DEBUG(msg) LOG("DEBUG", msg)
