#pragma once

#include <Windows.h>

namespace Constants {
    // Named pipe settings
    constexpr const char* DEFAULT_PIPE_NAME = "\\\\.\\pipe\\vibedbg";
    constexpr DWORD PIPE_BUFFER_SIZE = 4096;
    
    // Timeout settings (in milliseconds)
    constexpr unsigned int DEFAULT_TIMEOUT_MS = 30000;
    constexpr unsigned int LONG_TIMEOUT_MS = 60000;
    
    // Performance settings
    constexpr size_t MAX_OUTPUT_SIZE = 1048576; // 1MB
    constexpr size_t MAX_COMMAND_LENGTH = 4096;
    constexpr size_t MAX_MESSAGE_SIZE = 1048576; // 1MB
    
    // Server information
    constexpr const char* SERVER_VERSION = "VibeDbg v1.0.0";
    constexpr const char* EXTENSION_NAME = "VibeDbg WinDbg Extension";
}