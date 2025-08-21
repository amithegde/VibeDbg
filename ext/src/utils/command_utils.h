#pragma once

#include "../pch.h"
#include <string_view>
#include <string>
#include <algorithm>
#include <cctype>

namespace vibedbg::utils {

/**
 * Common utility functions for command processing and validation
 */
class CommandUtils {
public:
    // Command validation
    static bool is_command_safe(std::string_view command);
    static bool is_empty_result_expected(std::string_view command);
    static std::string normalize_command(std::string_view command);
    
    // Result formatting
    static std::string format_success_message(std::string_view command, const std::string& output);
    static std::string format_error_message(const std::string& error, const std::string& context = "");
    
    // String utilities
    static std::string trim(std::string_view input);
    static std::string to_lower(std::string_view input);
    
    // Debug logging helpers
    static void log_command_start(std::string_view command);
    static void log_command_result(std::string_view command, bool success, size_t output_length);
    
private:
    static const std::vector<std::string> DANGEROUS_COMMANDS;
    static const std::vector<std::string> EMPTY_OUTPUT_COMMANDS;
};

} // namespace vibedbg::utils