#pragma once

#include <Windows.h>
#include <string_view>
#include <chrono>

namespace vibedbg::constants {
    // Named pipe settings
    constexpr std::string_view DEFAULT_PIPE_NAME = R"(\\.\pipe\vibedbg_debug)";
    constexpr DWORD PIPE_BUFFER_SIZE = 64 * 1024;  // 64KB buffer
    constexpr DWORD MAX_PIPE_INSTANCES = 10;
    
    // Timeout settings (in milliseconds)
    constexpr std::chrono::milliseconds DEFAULT_TIMEOUT{30000};    // 30 seconds
    constexpr std::chrono::milliseconds QUICK_TIMEOUT{5000};       // 5 seconds
    constexpr std::chrono::milliseconds LONG_TIMEOUT{60000};       // 60 seconds
    constexpr std::chrono::milliseconds VERY_LONG_TIMEOUT{120000}; // 2 minutes
    
    // Command categorized timeouts
    constexpr std::chrono::milliseconds VERSION_TIMEOUT{5000};
    constexpr std::chrono::milliseconds EXECUTION_TIMEOUT{30000};
    constexpr std::chrono::milliseconds ANALYSIS_TIMEOUT{60000};
    constexpr std::chrono::milliseconds BULK_TIMEOUT{120000};
    
    // Performance settings
    constexpr size_t MAX_OUTPUT_SIZE = 1024 * 1024;  // 1MB max output
    constexpr size_t DEFAULT_CHUNK_SIZE = 4096;      // 4KB chunks
    constexpr size_t MAX_COMMAND_LENGTH = 2048;      // 2KB max command
    
    // Server information
    constexpr std::string_view EXTENSION_NAME = "VibeDbg";
    constexpr std::string_view EXTENSION_VERSION = "1.0.0";
    constexpr std::string_view EXTENSION_DESCRIPTION = "AI-powered WinDbg debugging extension";
    
    // Protocol version
    constexpr uint32_t PROTOCOL_VERSION = 1;
    
    // Error messages
    namespace errors {
        constexpr std::string_view NOT_INITIALIZED = "VibeDbg extension not initialized";
        constexpr std::string_view CONNECTION_FAILED = "Failed to connect to named pipe";
        constexpr std::string_view COMMAND_FAILED = "Command execution failed";
        constexpr std::string_view INVALID_PARAMETER = "Invalid parameter";
        constexpr std::string_view TIMEOUT_EXCEEDED = "Operation timed out";
    }
}