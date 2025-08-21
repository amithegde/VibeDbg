#include "pch.h"
#include "../../inc/message_protocol.h"
#include "../../inc/error_handling.h"
#include <random>
#include <iomanip>
#include <sstream>

using namespace vibedbg::communication;
using json = nlohmann::json;

std::vector<std::byte> MessageProtocol::serialize_command(const CommandRequest& request, ErrorCode* error) {
    try {
        json message_json = {
            {"type", "command"},
            {"request_id", request.request_id},
            {"command", request.command},
            {"parameters", request.parameters},
            {"timeout_ms", request.timeout.count()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                request.timestamp.time_since_epoch()).count()}
        };
        
        json full_message = {
            {"protocol_version", PROTOCOL_VERSION},
            {"message_type", static_cast<uint8_t>(MessageType::Command)},
            {"payload", message_json}
        };
        
        std::string json_str = full_message.dump();
        std::vector<std::byte> result;
        result.reserve(json_str.size() + MESSAGE_DELIMITER.size());
        
        std::transform(json_str.begin(), json_str.end(), std::back_inserter(result),
                      [](char c) { return static_cast<std::byte>(c); });
        
        // Add delimiter
        for (char c : MESSAGE_DELIMITER) {
            result.push_back(static_cast<std::byte>(c));
        }
        
        if (error) *error = ErrorCode::None;
        return result;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return {};
    }
}

std::vector<std::byte> MessageProtocol::serialize_response(const CommandResponse& response, ErrorCode* error) {
    try {
        json message_json = {
            {"type", "response"},
            {"request_id", response.request_id},
            {"success", response.success},
            {"output", response.output},
            {"error_message", response.error_message},
            {"execution_time_ms", response.execution_time.count()},
            {"session_data", response.session_data},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                response.timestamp.time_since_epoch()).count()}
        };
        
        json full_message = {
            {"protocol_version", PROTOCOL_VERSION},
            {"message_type", static_cast<uint8_t>(MessageType::Response)},
            {"payload", message_json}
        };
        
        std::string json_str = full_message.dump();
        std::vector<std::byte> result;
        result.reserve(json_str.size() + MESSAGE_DELIMITER.size());
        
        std::transform(json_str.begin(), json_str.end(), std::back_inserter(result),
                      [](char c) { return static_cast<std::byte>(c); });
        
        // Add delimiter
        for (char c : MESSAGE_DELIMITER) {
            result.push_back(static_cast<std::byte>(c));
        }
        
        if (error) *error = ErrorCode::None;
        return result;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return {};
    }
}

std::vector<std::byte> MessageProtocol::serialize_error(const ErrorMessage& error_msg, ErrorCode* error) {
    try {
        json message_json = {
            {"type", "error"},
            {"error_code", static_cast<uint32_t>(error_msg.error_code)},
            {"category", static_cast<uint8_t>(error_msg.category)},
            {"error_message", error_msg.error_message},
            {"suggestion", error_msg.suggestion},
            {"details", error_msg.details},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                error_msg.timestamp.time_since_epoch()).count()}
        };
        
        if (error_msg.request_id) {
            message_json["request_id"] = *error_msg.request_id;
        }
        
        json full_message = {
            {"protocol_version", PROTOCOL_VERSION},
            {"message_type", static_cast<uint8_t>(MessageType::Error)},
            {"payload", message_json}
        };
        
        std::string json_str = full_message.dump();
        std::vector<std::byte> result;
        result.reserve(json_str.size() + MESSAGE_DELIMITER.size());
        
        std::transform(json_str.begin(), json_str.end(), std::back_inserter(result),
                      [](char c) { return static_cast<std::byte>(c); });
        
        // Add delimiter
        for (char c : MESSAGE_DELIMITER) {
            result.push_back(static_cast<std::byte>(c));
        }
        
        if (error) *error = ErrorCode::None;
        return result;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return {};
    }
}

std::vector<std::byte> MessageProtocol::serialize_heartbeat(const HeartbeatMessage& heartbeat, ErrorCode* error) {
    try {
        json message_json = {
            {"type", "heartbeat"},
            {"session_info", heartbeat.session_info},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                heartbeat.timestamp.time_since_epoch()).count()}
        };
        
        json full_message = {
            {"protocol_version", PROTOCOL_VERSION},
            {"message_type", static_cast<uint8_t>(MessageType::Heartbeat)},
            {"payload", message_json}
        };
        
        std::string json_str = full_message.dump();
        std::vector<std::byte> result;
        result.reserve(json_str.size() + MESSAGE_DELIMITER.size());
        
        std::transform(json_str.begin(), json_str.end(), std::back_inserter(result),
                      [](char c) { return static_cast<std::byte>(c); });
        
        // Add delimiter
        for (char c : MESSAGE_DELIMITER) {
            result.push_back(static_cast<std::byte>(c));
        }
        
        if (error) *error = ErrorCode::None;
        return result;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return {};
    }
}

CommandRequest MessageProtocol::parse_command(std::span<const std::byte> data, ErrorCode* error) {
    CommandRequest request;
    
    try {
        ErrorCode json_error;
        auto parsed_json = bytes_to_json(data, &json_error);
        if (json_error != ErrorCode::None) {
            if (error) *error = json_error;
            return request;
        }
        
        // Validate message structure
        if (!parsed_json.contains("protocol_version") || 
            !parsed_json.contains("message_type") || 
            !parsed_json.contains("payload")) {
            if (error) *error = ErrorCode::InvalidMessage;
            return request;
        }
        
        auto& payload = parsed_json["payload"];
        if (!payload.contains("request_id") || !payload.contains("command")) {
            if (error) *error = ErrorCode::InvalidMessage;
            return request;
        }
        
        // Extract data
        request.request_id = payload["request_id"];
        request.command = payload["command"];
        
        if (payload.contains("parameters")) {
            request.parameters = payload["parameters"];
        }
        
        if (payload.contains("timeout_ms")) {
            request.timeout = std::chrono::milliseconds(payload["timeout_ms"]);
        }
        
        if (payload.contains("timestamp")) {
            auto timestamp_ms = std::chrono::milliseconds(payload["timestamp"]);
            request.timestamp = std::chrono::steady_clock::time_point(timestamp_ms);
        } else {
            request.timestamp = std::chrono::steady_clock::now();
        }
        
        if (error) *error = ErrorCode::None;
        return request;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return request;
    }
}

CommandResponse MessageProtocol::parse_response(std::span<const std::byte> data, ErrorCode* error) {
    CommandResponse response;
    
    try {
        ErrorCode json_error;
        auto parsed_json = bytes_to_json(data, &json_error);
        if (json_error != ErrorCode::None) {
            if (error) *error = json_error;
            return response;
        }
        
        // Validate message structure
        if (!parsed_json.contains("payload")) {
            if (error) *error = ErrorCode::InvalidMessage;
            return response;
        }
        
        auto& payload = parsed_json["payload"];
        
        // Extract data
        if (payload.contains("request_id")) {
            response.request_id = payload["request_id"];
        }
        
        if (payload.contains("success")) {
            response.success = payload["success"];
        }
        
        if (payload.contains("output")) {
            response.output = payload["output"];
        }
        
        if (payload.contains("error_message")) {
            response.error_message = payload["error_message"];
        }
        
        if (payload.contains("execution_time_ms")) {
            response.execution_time = std::chrono::milliseconds(payload["execution_time_ms"]);
        }
        
        if (payload.contains("session_data")) {
            response.session_data = payload["session_data"];
        }
        
        if (payload.contains("timestamp")) {
            auto timestamp_ms = std::chrono::milliseconds(payload["timestamp"]);
            response.timestamp = std::chrono::steady_clock::time_point(timestamp_ms);
        } else {
            response.timestamp = std::chrono::steady_clock::now();
        }
        
        if (error) *error = ErrorCode::None;
        return response;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return response;
    }
}

ErrorMessage MessageProtocol::parse_error(std::span<const std::byte> data, ErrorCode* error) {
    ErrorMessage error_message;
    
    try {
        ErrorCode json_error;
        auto parsed_json = bytes_to_json(data, &json_error);
        if (json_error != ErrorCode::None) {
            if (error) *error = json_error;
            return error_message;
        }
        
        // Validate message structure
        if (!parsed_json.contains("payload")) {
            if (error) *error = ErrorCode::InvalidMessage;
            return error_message;
        }
        
        auto& payload = parsed_json["payload"];
        
        // Extract data
        if (payload.contains("request_id")) {
            error_message.request_id = payload["request_id"];
        }
        
        if (payload.contains("error_code")) {
            error_message.error_code = static_cast<ErrorCode>(payload["error_code"]);
        }
        
        if (payload.contains("category")) {
            error_message.category = static_cast<ErrorCategory>(payload["category"]);
        }
        
        if (payload.contains("error_message")) {
            error_message.error_message = payload["error_message"];
        }
        
        if (payload.contains("suggestion")) {
            error_message.suggestion = payload["suggestion"];
        }
        
        if (payload.contains("details")) {
            error_message.details = payload["details"];
        }
        
        if (payload.contains("timestamp")) {
            auto timestamp_ms = std::chrono::milliseconds(payload["timestamp"]);
            error_message.timestamp = std::chrono::steady_clock::time_point(timestamp_ms);
        } else {
            error_message.timestamp = std::chrono::steady_clock::now();
        }
        
        if (error) *error = ErrorCode::None;
        return error_message;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return error_message;
    }
}

HeartbeatMessage MessageProtocol::parse_heartbeat(std::span<const std::byte> data, ErrorCode* error) {
    HeartbeatMessage heartbeat;
    
    try {
        ErrorCode json_error;
        auto parsed_json = bytes_to_json(data, &json_error);
        if (json_error != ErrorCode::None) {
            if (error) *error = json_error;
            return heartbeat;
        }
        
        // Validate message structure
        if (!parsed_json.contains("payload")) {
            if (error) *error = ErrorCode::InvalidMessage;
            return heartbeat;
        }
        
        auto& payload = parsed_json["payload"];
        
        // Extract data
        if (payload.contains("session_info")) {
            heartbeat.session_info = payload["session_info"];
        }
        
        if (payload.contains("timestamp")) {
            auto timestamp_ms = std::chrono::milliseconds(payload["timestamp"]);
            heartbeat.timestamp = std::chrono::steady_clock::time_point(timestamp_ms);
        } else {
            heartbeat.timestamp = std::chrono::steady_clock::now();
        }
        
        if (error) *error = ErrorCode::None;
        return heartbeat;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return heartbeat;
    }
}

MessageType MessageProtocol::get_message_type(std::span<const std::byte> data) {
    try {
        ErrorCode json_error;
        auto parsed_json = bytes_to_json(data, &json_error);
        if (json_error != ErrorCode::None) {
            return static_cast<MessageType>(0); // Invalid type
        }
        
        if (parsed_json.contains("message_type")) {
            return static_cast<MessageType>(parsed_json["message_type"]);
        }
        
        return static_cast<MessageType>(0); // Invalid type
    } catch (...) {
        return static_cast<MessageType>(0); // Invalid type
    }
}

bool MessageProtocol::validate_message_size(size_t size) {
    return size > 0 && size <= MAX_MESSAGE_SIZE;
}

std::string MessageProtocol::generate_request_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << '-';
        }
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

ErrorCategory MessageProtocol::classify_error(ErrorCode error_code) {
    switch (error_code) {
        case ErrorCode::InvalidParameter:
        case ErrorCode::UnknownCommand:
            return ErrorCategory::UserInput;
            
        case ErrorCode::Timeout:
        case ErrorCode::ConnectionLost:
            return ErrorCategory::Communication;
            
        case ErrorCode::ProcessNotFound:
        case ErrorCode::ThreadError:
            return ErrorCategory::Process;
            
        case ErrorCode::MemoryAccessError:
            return ErrorCategory::Memory;
            
        case ErrorCode::SymbolLoadError:
            return ErrorCategory::Symbol;
            
        case ErrorCode::ExtensionNotLoaded:
            return ErrorCategory::Extension;
            
        case ErrorCode::InternalError:
        default:
            return ErrorCategory::System;
    }
}

std::string MessageProtocol::get_error_suggestion(ErrorCode error_code) {
    switch (error_code) {
        case ErrorCode::InvalidMessage:
            return "Check message format and ensure it follows the protocol specification";
        case ErrorCode::CommandFailed:
            return "Verify the command syntax and try again";
        case ErrorCode::Timeout:
            return "Increase timeout value or check if the target is responsive";
        case ErrorCode::ExtensionNotLoaded:
            return "Load the VibeDbg extension first using the vibedbg_connect command";
        case ErrorCode::ProcessNotFound:
            return "Ensure the target process is running and accessible";
        case ErrorCode::MemoryAccessError:
            return "Check memory addresses and permissions";
        default:
            return "Check the logs for more detailed error information";
    }
}

ErrorMessage MessageProtocol::create_error_message(const std::string& request_id, ErrorCode error_code, 
                                         const std::string& message, const std::string& context) {
    ErrorMessage error_msg;
    error_msg.request_id = request_id;
    error_msg.error_code = error_code;
    error_msg.category = classify_error(error_code);
    error_msg.error_message = message;
    error_msg.suggestion = get_error_suggestion(error_code);
    error_msg.timestamp = std::chrono::steady_clock::now();
    
    if (!context.empty()) {
        error_msg.details = json{{"context", context}};
    }
    
    return error_msg;
}

// Private helper methods

json MessageProtocol::bytes_to_json(std::span<const std::byte> data, ErrorCode* error) {
    try {
        if (data.empty()) {
            if (error) *error = ErrorCode::InvalidMessage;
            return json{};
        }
        
        // Convert bytes to string
        std::string json_str;
        json_str.reserve(data.size());
        for (auto byte : data) {
            json_str.push_back(static_cast<char>(byte));
        }
        
        // Remove delimiter if present
        if (json_str.size() >= MESSAGE_DELIMITER.size()) {
            auto delimiter_pos = json_str.find(MESSAGE_DELIMITER);
            if (delimiter_pos != std::string::npos) {
                json_str = json_str.substr(0, delimiter_pos);
            }
        }
        
        // Parse JSON
        auto parsed = json::parse(json_str);
        
        if (error) *error = ErrorCode::None;
        return parsed;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return json{};
    }
}

std::vector<std::byte> MessageProtocol::json_to_bytes(const json& j) {
    std::string json_str = j.dump();
    std::vector<std::byte> result;
    result.reserve(json_str.size());
    
    std::transform(json_str.begin(), json_str.end(), std::back_inserter(result),
                  [](char c) { return static_cast<std::byte>(c); });
    
    return result;
}

std::vector<std::byte> MessageProtocol::add_header(MessageType type, std::span<const std::byte> payload) {
    json header = {
        {"protocol_version", PROTOCOL_VERSION},
        {"message_type", static_cast<uint8_t>(type)},
        {"payload_size", payload.size()}
    };
    
    auto header_bytes = json_to_bytes(header);
    std::vector<std::byte> result;
    result.reserve(header_bytes.size() + payload.size());
    
    result.insert(result.end(), header_bytes.begin(), header_bytes.end());
    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

std::span<const std::byte> MessageProtocol::extract_payload(std::span<const std::byte> data, ErrorCode* error) {
    try {
        // For simplified implementation, just return the data as-is
        // In a more complex implementation, this would parse headers
        if (error) *error = ErrorCode::None;
        return data;
    } catch (...) {
        if (error) *error = ErrorCode::InvalidMessage;
        return {};
    }
}