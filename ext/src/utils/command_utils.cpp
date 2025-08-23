#include "pch.h"
#include "command_utils.h"

using namespace vibedbg::utils;

const std::vector<std::string> CommandUtils::DANGEROUS_COMMANDS = {
    "format", "del", "rmdir", "erase", "delete"
};

const std::vector<std::string> CommandUtils::EMPTY_OUTPUT_COMMANDS = {
    "bp", "ba", "bu", "bm", "g", "gh", "gn", "gu", "p", "t", "bc", "bd", "be"
};

const std::vector<std::string> CommandUtils::VISUALIZATION_COMMANDS = {
    "dx"
};

bool CommandUtils::is_command_safe(std::string_view command) {
    if (command.empty() || command.find_first_not_of(" \t\n\r") == std::string::npos) {
        return false;
    }
    
    std::string lower_cmd = to_lower(command);
    
    for (const auto& dangerous : DANGEROUS_COMMANDS) {
        if (lower_cmd.find(dangerous) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool CommandUtils::is_empty_result_expected(std::string_view command) {
    std::string lower_cmd = to_lower(trim(command));
    
    for (const auto& empty_cmd : EMPTY_OUTPUT_COMMANDS) {
        if (lower_cmd.starts_with(empty_cmd + " ") || lower_cmd == empty_cmd) {
            return true;
        }
    }
    
    return false;
}

bool CommandUtils::is_visualization_command(std::string_view command) {
    std::string lower_cmd = to_lower(trim(command));
    
    for (const auto& vis_cmd : VISUALIZATION_COMMANDS) {
        if (lower_cmd.starts_with(vis_cmd + " ") || lower_cmd == vis_cmd) {
            return true;
        }
    }
    
    return false;
}

std::string CommandUtils::normalize_command(std::string_view command) {
    return trim(command);
}

std::string CommandUtils::format_success_message(std::string_view command, const std::string& output) {
    if (output.empty()) {
        if (is_empty_result_expected(command)) {
            return "Command executed successfully";
        } else {
            return "Command executed successfully";
        }
    }
    return output;
}

std::string CommandUtils::format_error_message(const std::string& error, const std::string& context) {
    if (context.empty()) {
        return error.empty() ? "Unknown error" : "Error: " + error;
    }
    return "Error in " + context + ": " + (error.empty() ? "Unknown error" : error);
}

std::string CommandUtils::trim(std::string_view input) {
    size_t start = input.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = input.find_last_not_of(" \t\n\r");
    return std::string(input.substr(start, end - start + 1));
}

std::string CommandUtils::to_lower(std::string_view input) {
    std::string result(input);
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

void CommandUtils::log_command_start(std::string_view command) {
    LOG_INFO("CommandUtils", "Executing command: " + std::string(command));
}

void CommandUtils::log_command_result(std::string_view command, bool success, size_t output_length) {
    if (success) {
        LOG_INFO_DETAIL("CommandUtils", "Command executed successfully", "Command: " + std::string(command) + ", Output length: " + std::to_string(output_length));
    } else {
        LOG_ERROR_DETAIL("CommandUtils", "Command execution failed", "Command: " + std::string(command) + ", Output length: " + std::to_string(output_length));
    }
}