#pragma once

namespace vibedbg::constants {

// Command limits
constexpr size_t MAX_COMMAND_LENGTH = 4096;
constexpr size_t MAX_OUTPUT_LENGTH = 1024 * 1024; // 1MB

// Message limits
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB

// Timeout values
constexpr int DEFAULT_COMMAND_TIMEOUT_MS = 30000; // 30 seconds
constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 5000; // 5 seconds

// Named pipe settings
constexpr int MAX_PIPE_INSTANCES = 10;
constexpr int PIPE_BUFFER_SIZE = 65536; // 64KB

// Session limits
constexpr int MAX_CONCURRENT_SESSIONS = 5;

} // namespace vibedbg::constants