#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>
#include <memory>
#include "json.h"

namespace vibedbg::communication {

using json = nlohmann::json;

enum class MessageType : uint8_t {
    Command = 1,
    Response = 2,
    Error = 3,
    Heartbeat = 4
};

enum class ErrorCode : uint32_t {
    None = 0,
    InvalidMessage = 1,
    CommandFailed = 2,
    Timeout = 3,
    ConnectionLost = 4,
    InvalidParameter = 5,
    UnknownCommand = 6,
    ExtensionNotLoaded = 7,
    SymbolLoadError = 8,
    MemoryAccessError = 9,
    ProcessNotFound = 10,
    ThreadError = 11,
    BreakpointError = 12,
    StackError = 13,
    ModuleError = 14,
    DebuggingContextError = 15,
    InternalError = 16,
    AlreadyStarted = 17,
    ClientNotFound = 18,
    ClientNotConnected = 19,
    SendFailed = 20,
    HandlerException = 21,
    PipeCreationFailed = 22
};

struct CommandRequest {
    std::string request_id;
    std::string command;
    json parameters;
    std::chrono::milliseconds timeout{30000};
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

struct CommandResponse {
    std::string request_id;
    bool success{false};
    std::string output;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
    json session_data;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

enum class ErrorCategory : uint8_t {
    Unknown = 0,
    UserInput = 1,
    System = 2,
    Communication = 3,
    Process = 4,
    Memory = 5,
    Symbol = 6,
    Extension = 7,
    Timeout = 8
};

struct ErrorMessage {
    std::optional<std::string> request_id;
    ErrorCode error_code{ErrorCode::None};
    ErrorCategory category{ErrorCategory::Unknown};
    std::string error_message;
    std::string suggestion;
    json details;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

struct HeartbeatMessage {
    json session_info;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

class MessageProtocol {
public:
    static constexpr uint32_t PROTOCOL_VERSION = 1;
    static constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB
    static constexpr std::string_view MESSAGE_DELIMITER = "\r\n\r\n";

    // Serialization
    static std::vector<std::byte> serialize_command(const CommandRequest& request, ErrorCode* error = nullptr);
    
    static std::vector<std::byte> serialize_response(const CommandResponse& response, ErrorCode* error = nullptr);
    
    static std::vector<std::byte> serialize_error(const ErrorMessage& error_msg, ErrorCode* error = nullptr);
    
    static std::vector<std::byte> serialize_heartbeat(const HeartbeatMessage& heartbeat, ErrorCode* error = nullptr);

    // Deserialization
    static CommandRequest parse_command(std::span<const std::byte> data, ErrorCode* error = nullptr);
    
    static CommandResponse parse_response(std::span<const std::byte> data, ErrorCode* error = nullptr);
    
    static ErrorMessage parse_error(std::span<const std::byte> data, ErrorCode* error = nullptr);
    
    static HeartbeatMessage parse_heartbeat(std::span<const std::byte> data, ErrorCode* error = nullptr);

    // Utility functions
    static MessageType get_message_type(std::span<const std::byte> data);
    static bool validate_message_size(size_t size);
    static std::string generate_request_id();
    
    // Error handling utilities
    static ErrorCategory classify_error(ErrorCode error_code);
    static std::string get_error_suggestion(ErrorCode error_code);
    static ErrorMessage create_error_message(const std::string& request_id, ErrorCode error_code, 
                                           const std::string& message, const std::string& context = "");

private:
    static json bytes_to_json(std::span<const std::byte> data, ErrorCode* error = nullptr);
    static std::vector<std::byte> json_to_bytes(const json& j);
    static std::vector<std::byte> add_header(MessageType type, std::span<const std::byte> payload);
    static std::span<const std::byte> extract_payload(std::span<const std::byte> data, ErrorCode* error = nullptr);
};

} // namespace vibedbg::communication