#include "pch.h"
#include "../../inc/logging.h"
#include <sstream>
#include <iomanip>

using namespace vibedbg::logging;





// Static member initialization
std::string Logger::component_name_ = "VibeDbg";
std::mutex Logger::log_mutex_;
bool Logger::initialized_ = false;

void Logger::Initialize(const std::string& component_name) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (!initialized_) {
        component_name_ = component_name;
        initialized_ = true;
        // Don't call logging functions from within Initialize to avoid deadlock
        // Just use OutputDebugString directly
        OutputDebugStringA(("[" + component_name_ + "] Logging system initialized\n").c_str());
    }
}

void Logger::Cleanup() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (initialized_) {
        // Don't call logging functions from within Cleanup to avoid deadlock
        OutputDebugStringA(("[" + component_name_ + "] Logging system shutting down\n").c_str());
        initialized_ = false;
    }
}

// Simplified Log methods (no Level enum parameter in public interface anymore)
void Logger::Trace(const std::string& context, const std::string& message) {
    Log("TRACE", context, message, "");
}

void Logger::Debug(const std::string& context, const std::string& message) {
    Log("DEBUG", context, message, "");
}

void Logger::Info(const std::string& context, const std::string& message) {
    Log("INFO", context, message, "");
}

void Logger::Warning(const std::string& context, const std::string& message) {
    Log("WARNING", context, message, "");
}

void Logger::Error(const std::string& context, const std::string& message) {
    Log("ERROR", context, message, "");
}

void Logger::Fatal(const std::string& context, const std::string& message) {
    Log("FATAL", context, message, "");
}

// Logging with details
void Logger::Trace(const std::string& context, const std::string& message, const std::string& details) {
    Log("TRACE", context, message, details);
}

void Logger::Debug(const std::string& context, const std::string& message, const std::string& details) {
    Log("DEBUG", context, message, details);
}

void Logger::Info(const std::string& context, const std::string& message, const std::string& details) {
    Log("INFO", context, message, details);
}

void Logger::Warning(const std::string& context, const std::string& message, const std::string& details) {
    Log("WARNING", context, message, details);
}

void Logger::Error(const std::string& context, const std::string& message, const std::string& details) {
    Log("ERROR", context, message, details);
}

void Logger::Fatal(const std::string& context, const std::string& message, const std::string& details) {
    Log("FATAL", context, message, details);
}

// WinDbg UI logging methods - these output to WinDbg console
void Logger::LogToWinDbg(const std::string& context, const std::string& message) {
    // Safety check for ExtensionApis before using dprintf
    if (ExtensionApis.lpOutputRoutine != nullptr) {
        dprintf("VibeDbg [%s]: %s\n", context.c_str(), message.c_str());
    } else {
        // Fallback to OutputDebugString if dprintf not available
        OutputDebugStringA(("VibeDbg [" + context + "]: " + message + "\n").c_str());
    }
}

void Logger::LogToWinDbg(const std::string& context, const std::string& message, const std::string& details) {
    // Safety check for ExtensionApis before using dprintf
    if (ExtensionApis.lpOutputRoutine != nullptr) {
        if (details.empty()) {
            dprintf("VibeDbg [%s]: %s\n", context.c_str(), message.c_str());
        } else {
            dprintf("VibeDbg [%s]: %s | %s\n", context.c_str(), message.c_str(), details.c_str());
        }
    } else {
        // Fallback to OutputDebugString if dprintf not available
        std::string full_message = "VibeDbg [" + context + "]: " + message;
        if (!details.empty()) {
            full_message += " | " + details;
        }
        full_message += "\n";
        OutputDebugStringA(full_message.c_str());
    }
}

// Internal helper to format and write
void Logger::Log(const std::string& level_str, const std::string& context, const std::string& message, const std::string& details) {
    if (!initialized_) {
        Initialize();
    }
    
    std::string formatted_message = FormatLogMessage(level_str, context, message, details);
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    WriteToDebugView(formatted_message);
}



std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::WriteToDebugView(const std::string& message) {
    // OutputDebugString is the key function that DebugView captures
    OutputDebugStringA(message.c_str());
}

std::string Logger::FormatLogMessage(const std::string& level, const std::string& context, const std::string& message, const std::string& details) {
    std::ostringstream oss;
    
    // Format: [Timestamp] [Level] [Component] [Context] Message [Details]
    oss << "[" << GetTimestamp() << "] ";
    oss << "[" << level << "] ";
    oss << "[" << component_name_ << "] ";
    oss << "[" << context << "] ";
    oss << message;
    
    if (!details.empty()) {
        oss << " | " << details;
    }
    
    return oss.str();
}
