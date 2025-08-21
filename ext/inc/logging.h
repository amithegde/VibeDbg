#pragma once

#include <string>
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

namespace vibedbg::logging {



class Logger {
public:
    static void Initialize(const std::string& component_name = "VibeDbg");
    static void Cleanup();
    
    // Simple logging methods that use OutputDebugString
    static void Trace(const std::string& context, const std::string& message);
    static void Debug(const std::string& context, const std::string& message);
    static void Info(const std::string& context, const std::string& message);
    static void Warning(const std::string& context, const std::string& message);
    static void Error(const std::string& context, const std::string& message);
    static void Fatal(const std::string& context, const std::string& message);
    
    // Logging with details
    static void Trace(const std::string& context, const std::string& message, const std::string& details);
    static void Debug(const std::string& context, const std::string& message, const std::string& details);
    static void Info(const std::string& context, const std::string& message, const std::string& details);
    static void Warning(const std::string& context, const std::string& message, const std::string& details);
    static void Error(const std::string& context, const std::string& message, const std::string& details);
    static void Fatal(const std::string& context, const std::string& message, const std::string& details);
    

    
    // WinDbg UI logging methods (these show up in WinDbg UI)
    static void LogToWinDbg(const std::string& context, const std::string& message);
    static void LogToWinDbg(const std::string& context, const std::string& message, const std::string& details);
    
private:
    static std::string GetTimestamp();
    static void WriteToDebugView(const std::string& message);
    static std::string FormatLogMessage(const std::string& level, const std::string& context, const std::string& message, const std::string& details = "");
    static void Log(const std::string& level_str, const std::string& context, const std::string& message, const std::string& details = "");
    
    static std::string component_name_;
    static std::mutex log_mutex_;
    static bool initialized_;
};

// Macro for easier logging with automatic context
#define LOG_TRACE(context, message) vibedbg::logging::Logger::Trace(context, message)
#define LOG_DEBUG(context, message) vibedbg::logging::Logger::Debug(context, message)
#define LOG_INFO(context, message) vibedbg::logging::Logger::Info(context, message)
#define LOG_WARNING(context, message) vibedbg::logging::Logger::Warning(context, message)
#define LOG_ERROR(context, message) vibedbg::logging::Logger::Error(context, message)
#define LOG_FATAL(context, message) vibedbg::logging::Logger::Fatal(context, message)

// Macro for logging with details
#define LOG_TRACE_DETAIL(context, message, details) vibedbg::logging::Logger::Trace(context, message, details)
#define LOG_DEBUG_DETAIL(context, message, details) vibedbg::logging::Logger::Debug(context, message, details)
#define LOG_INFO_DETAIL(context, message, details) vibedbg::logging::Logger::Info(context, message, details)
#define LOG_WARNING_DETAIL(context, message, details) vibedbg::logging::Logger::Warning(context, message, details)
#define LOG_ERROR_DETAIL(context, message, details) vibedbg::logging::Logger::Error(context, message, details)
#define LOG_FATAL_DETAIL(context, message, details) vibedbg::logging::Logger::Fatal(context, message, details)

// WinDbg UI logging macros
#define LOG_WINDBG(context, message) vibedbg::logging::Logger::LogToWinDbg(context, message)
#define LOG_WINDBG_DETAIL(context, message, details) vibedbg::logging::Logger::LogToWinDbg(context, message, details)



} // namespace vibedbg::logging
