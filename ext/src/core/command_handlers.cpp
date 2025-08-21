#include "pch.h"
#include "command_handlers.h"
#include "constants.h"
#include "../utils/windbg_helpers.h"
#include "../utils/command_utils.h"
#include <regex>
#include <sstream>

using namespace vibedbg::core;
using namespace vibedbg::constants;
using namespace vibedbg::utils;

// Simple string formatting utilities
namespace {
    std::string format_hex(uintptr_t value) {
        std::ostringstream oss;
        oss << "0x" << std::hex << value;
        return oss.str();
    }
    
    std::string format_version() {
        std::ostringstream oss;
        oss << EXTENSION_NAME << " v" << EXTENSION_VERSION << "\n" << EXTENSION_DESCRIPTION;
        return oss.str();
    }
}

/**
 * @brief Constructs a command handlers instance with session manager and command executor.
 * 
 * @param[in] session_manager Shared pointer to the session manager for session operations
 * @param[in] command_executor Shared pointer to the command executor for WinDbg command execution
 */
CommandHandlers::CommandHandlers(
    _In_ std::shared_ptr<SessionManager> session_manager,
    _In_ std::shared_ptr<CommandExecutor> command_executor)
    : session_manager_(std::move(session_manager))
    , command_executor_(std::move(command_executor)) {
}

/**
 * @brief Handles version information requests.
 * 
 * @return Formatted version string containing extension name, version, and description
 */
std::string CommandHandlers::handle_version() {
    return format_version();
}

/**
 * @brief Handles status information requests.
 * 
 * @return Formatted status string containing current session information
 */
std::string CommandHandlers::handle_status() {
    return format_session_status();
}

/**
 * @brief Handles help information requests.
 * 
 * This method returns comprehensive help information including all available
 * commands, their usage, and examples for common debugging scenarios.
 * 
 * @return Detailed help text with command descriptions and examples
 */
std::string CommandHandlers::handle_help() {
    return R"(VibeDbg Command Help:

Basic Commands:
  version              - Show extension version
  status               - Show current status
  help                 - Show this help

Session Management:
  session_info         - Show session information
  mode_detection       - Detect current debugging mode

Process Management:
  list_processes       - List all processes
  attach_process <pid> - Attach to process
  detach_process       - Detach from current process
  create_process <path> - Create new process for debugging
  restart_process      - Restart target process
  terminate_process    - Terminate target process

Thread Management:
  list_threads         - List all threads
  thread_info <tid>    - Show thread information
  switch_thread <tid>  - Switch to thread

Breakpoint Management:
  set_breakpoint <addr>     - Set breakpoint at address
  set_symbol_breakpoint <symbol> - Set breakpoint at symbol
  set_access_breakpoint <type> <addr> - Set access breakpoint
  clear_breakpoint <id>     - Clear breakpoint
  disable_breakpoint <id>   - Disable breakpoint
  enable_breakpoint <id>    - Enable breakpoint
  list_breakpoints          - List all breakpoints

Execution Control:
  continue_execution        - Continue execution (g)
  step_over                 - Step over instruction (p)
  step_into                 - Step into function (t)
  step_out                  - Step out of function (gu)
  continue_exception_handled - Continue with exception handled (gh)
  continue_exception_not_handled - Continue with exception not handled (gn)

Memory Operations:
  read_memory <addr> <size>    - Read memory
  display_memory <addr> <size> - Display memory with formatting
  search_memory <start> <end> <pattern> - Search memory for pattern
  show_memory_region <addr>    - Show memory region information

Module Operations:
  list_modules         - List loaded modules
  module_info <name>   - Show module information
  load_symbols <module> - Load symbols for module
  show_symbol_info <symbol> - Show symbol information

Stack Operations:
  stack_trace          - Show stack trace
  call_stack           - Show call stack
  show_registers       - Show current registers

Crash Dump Analysis:
  load_dump <path>     - Load crash dump file
  analyze_crash        - Analyze crash dump
  analyze_deadlock     - Analyze deadlock scenario

Direct Execution:
  execute <command>    - Execute WinDbg command directly

Examples:
  # Debug a new process
  create_process C:\dev\cpp\Hello\x64\Release\Hello.exe
  set_symbol_breakpoint main
  continue_execution
  
  # Analyze crash dump
  load_dump C:\crashes\app.dmp
  analyze_crash
  
  # Debug deadlock
  analyze_deadlock
)";
}

/**
 * @brief Handles session information requests.
 * 
 * @return JSON-formatted session information
 */
std::string CommandHandlers::handle_session_info() {
    return format_session_json();
}

/**
 * @brief Handles mode detection requests.
 * 
 * @return String describing the current debugging mode
 */
std::string CommandHandlers::handle_mode_detection() {
    return "Current mode: User Mode (user-mode debugging only)";
}

/**
 * @brief Handles direct command execution requests.
 * 
 * This method provides a safe interface for executing WinDbg commands
 * through the command executor. It includes command validation, logging,
 * and proper error handling.
 * 
 * @param[in] command The WinDbg command to execute
 * 
 * @return Formatted result of command execution
 */
std::string CommandHandlers::handle_execute_command(_In_ std::string_view command) {
    CommandUtils::log_command_start(command);
    
    if (!command_executor_) {
        return CommandUtils::format_error_message("Internal error");
    }
    
    if (!CommandUtils::is_command_safe(command)) {
        return CommandUtils::format_error_message("Invalid or unsafe command");
    }
    
    ExecutionOptions options;
    ExecutionError error = ExecutionError::None;
    auto result = command_executor_->execute_command(command, options, &error);
    
    bool success = (error == ExecutionError::None && result.success);
    CommandUtils::log_command_result(command, success, result.output.length());
    
    if (success) {
        return CommandUtils::format_success_message(command, result.output);
    } else {
        return CommandUtils::format_error_message(result.error_message, "command execution");
    }
}

/**
 * @brief Handles process listing requests.
 * 
 * @return Formatted list of all processes
 */
std::string CommandHandlers::handle_list_processes() {
    // Use command executor to run process list command
    auto result = handle_execute_command("!process 0 0");
    if (result.empty()) {
        return "Error listing processes";
    }
    
    // For now, return raw output. In a full implementation, 
    // we would parse this into structured data
    return result;
}

/**
 * @brief Handles module listing requests.
 * 
 * @return Formatted list of loaded modules
 */
std::string CommandHandlers::handle_list_modules() {
    auto result = handle_execute_command("lm");
    if (result.empty()) {
        return "Error listing modules";
    }
    
    return result;
}

/**
 * @brief Handles thread listing requests.
 * 
 * @return Formatted list of all threads
 */
std::string CommandHandlers::handle_list_threads() {
    auto result = handle_execute_command("~");
    if (result.empty()) {
        return "Error listing threads";
    }
    
    return result;
}

/**
 * @brief Handles stack trace requests.
 * 
 * @return Formatted stack trace information
 */
std::string CommandHandlers::handle_stack_trace() {
    auto result = handle_execute_command("k");
    if (result.empty()) {
        return "Error getting stack trace";
    }
    
    return result;
}

/**
 * @brief Handles call stack requests.
 * 
 * @return Formatted call stack information
 */
std::string CommandHandlers::handle_call_stack() {
    auto result = handle_execute_command("kn");
    if (result.empty()) {
        return "Error getting call stack";
    }
    
    return result;
}

/**
 * @brief Handles memory reading requests.
 * 
 * @param[in] address Memory address to read from
 * @param[in] size Number of bytes to read
 * 
 * @return Formatted memory dump
 */
std::string CommandHandlers::handle_read_memory(_In_ uintptr_t address, _In_ size_t size) {
    std::ostringstream oss;
    oss << "db " << format_hex(address) << " L" << format_hex(size);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles memory display requests.
 * 
 * @param[in] address Memory address to display
 * @param[in] size Number of bytes to display
 * 
 * @return Formatted memory display
 */
std::string CommandHandlers::handle_display_memory(_In_ uintptr_t address, _In_ size_t size) {
    std::ostringstream oss;
    oss << "dd " << format_hex(address) << " L" << format_hex(size / 4);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles process attachment requests.
 * 
 * @param[in] process_id Process ID to attach to
 * 
 * @return Result of attachment operation
 */
std::string CommandHandlers::handle_attach_process(_In_ uint32_t process_id) {
    std::ostringstream oss;
    oss << ".attach " << format_hex(process_id);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles process detachment requests.
 * 
 * @return Result of detachment operation
 */
std::string CommandHandlers::handle_detach_process() {
    return handle_execute_command(".detach");
}

/**
 * @brief Handles thread information requests.
 * 
 * @param[in] thread_id Thread ID to get information for
 * 
 * @return Formatted thread information including registers
 */
std::string CommandHandlers::handle_thread_info(_In_ uint32_t thread_id) {
    std::ostringstream oss;
    oss << "~" << thread_id << "s";
    auto result = handle_execute_command(oss.str());
    if (result.empty()) {
        return "No thread information available";
    }
    return result;
}

/**
 * @brief Handles thread switching requests.
 * 
 * @param[in] thread_id Thread ID to switch to
 * 
 * @return Result of thread switching operation
 */
std::string CommandHandlers::handle_switch_thread(_In_ uint32_t thread_id) {
    std::ostringstream oss;
    oss << "~" << thread_id << "s";
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles module information requests.
 * 
 * @param[in] module_name Name of the module to get information for
 * 
 * @return Formatted module information
 */
std::string CommandHandlers::handle_module_info(_In_ std::string_view module_name) {
    std::ostringstream oss;
    oss << "lm m " << module_name;
    return handle_execute_command(oss.str());
}

// Breakpoint Management Commands

/**
 * @brief Handles breakpoint setting at address requests.
 * 
 * @param[in] address Memory address to set breakpoint at
 * 
 * @return Result of breakpoint setting operation
 */
std::string CommandHandlers::handle_set_breakpoint(_In_ uintptr_t address) {
    std::ostringstream oss;
    oss << "bp " << format_hex(address);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles breakpoint setting at symbol requests.
 * 
 * @param[in] symbol Symbol name to set breakpoint at
 * 
 * @return Result of breakpoint setting operation
 */
std::string CommandHandlers::handle_set_symbol_breakpoint(_In_ std::string_view symbol) {
    std::ostringstream oss;
    oss << "bp " << symbol;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles access breakpoint setting requests.
 * 
 * @param[in] address Memory address to set breakpoint at
 * @param[in] access_type Type of access to break on (r, w, e)
 * 
 * @return Result of breakpoint setting operation
 */
std::string CommandHandlers::handle_set_access_breakpoint(
    _In_ uintptr_t address, 
    _In_ std::string_view access_type) {
    std::ostringstream oss;
    oss << "ba " << access_type << " " << format_hex(address);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles breakpoint clearing requests.
 * 
 * @param[in] breakpoint_id ID of the breakpoint to clear
 * 
 * @return Result of breakpoint clearing operation
 */
std::string CommandHandlers::handle_clear_breakpoint(_In_ uint32_t breakpoint_id) {
    std::ostringstream oss;
    oss << "bc " << breakpoint_id;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles breakpoint disabling requests.
 * 
 * @param[in] breakpoint_id ID of the breakpoint to disable
 * 
 * @return Result of breakpoint disabling operation
 */
std::string CommandHandlers::handle_disable_breakpoint(_In_ uint32_t breakpoint_id) {
    std::ostringstream oss;
    oss << "bd " << breakpoint_id;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles breakpoint enabling requests.
 * 
 * @param[in] breakpoint_id ID of the breakpoint to enable
 * 
 * @return Result of breakpoint enabling operation
 */
std::string CommandHandlers::handle_enable_breakpoint(_In_ uint32_t breakpoint_id) {
    std::ostringstream oss;
    oss << "be " << breakpoint_id;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles breakpoint listing requests.
 * 
 * @return Formatted list of all breakpoints
 */
std::string CommandHandlers::handle_list_breakpoints() {
    return handle_execute_command("bl");
}

// Execution Control Commands

/**
 * @brief Handles execution continuation requests.
 * 
 * @return Result of execution continuation
 */
std::string CommandHandlers::handle_continue_execution() {
    return handle_execute_command("g");
}

/**
 * @brief Handles step over requests.
 * 
 * @return Result of step over operation
 */
std::string CommandHandlers::handle_step_over() {
    return handle_execute_command("p");
}

/**
 * @brief Handles step into requests.
 * 
 * @return Result of step into operation
 */
std::string CommandHandlers::handle_step_into() {
    return handle_execute_command("t");
}

/**
 * @brief Handles step out requests.
 * 
 * @return Result of step out operation
 */
std::string CommandHandlers::handle_step_out() {
    return handle_execute_command("gu");
}

/**
 * @brief Handles continue with exception handled requests.
 * 
 * @return Result of continue with exception handled operation
 */
std::string CommandHandlers::handle_continue_with_exception_handled() {
    return handle_execute_command("gh");
}

/**
 * @brief Handles continue with exception not handled requests.
 * 
 * @return Result of continue with exception not handled operation
 */
std::string CommandHandlers::handle_continue_with_exception_not_handled() {
    return handle_execute_command("gn");
}

// Process Management Commands

/**
 * @brief Handles process creation requests.
 * 
 * @param[in] process_path Path to the process executable to create
 * 
 * @return Result of process creation operation
 */
std::string CommandHandlers::handle_create_process(_In_ std::string_view process_path) {
    std::ostringstream oss;
    oss << ".create " << process_path;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles process restart requests.
 * 
 * @return Result of process restart operation
 */
std::string CommandHandlers::handle_restart_process() {
    return handle_execute_command(".restart");
}

/**
 * @brief Handles process termination requests.
 * 
 * @return Result of process termination operation
 */
std::string CommandHandlers::handle_terminate_process() {
    return handle_execute_command(".kill");
}

// Crash Dump Analysis Commands

/**
 * @brief Handles crash dump loading requests.
 * 
 * @param[in] dump_path Path to the crash dump file to load
 * 
 * @return Result of dump loading operation
 */
std::string CommandHandlers::handle_load_dump(_In_ std::string_view dump_path) {
    std::ostringstream oss;
    oss << ".dump " << dump_path;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles crash analysis requests.
 * 
 * @return Detailed crash analysis information
 */
std::string CommandHandlers::handle_analyze_crash() {
    return handle_execute_command("!analyze -v");
}

/**
 * @brief Handles deadlock analysis requests.
 * 
 * This method performs comprehensive deadlock analysis including:
 * - Thread information and states
 * - Stack traces for all threads
 * - Lock analysis
 * - Critical section analysis
 * 
 * @return Comprehensive deadlock analysis report
 */
std::string CommandHandlers::handle_analyze_deadlock() {
    // Specific analysis for deadlock scenarios
    std::string result;
    
    // Get thread information
    result += "=== Thread Analysis ===\n";
    result += handle_execute_command("~");
    result += "\n\n";
    
    // Get stack traces for all threads
    result += "=== Stack Traces ===\n";
    result += handle_execute_command("~*k");
    result += "\n\n";
    
    // Check for locks and synchronization objects
    result += "=== Lock Analysis ===\n";
    result += handle_execute_command("!locks");
    result += "\n\n";
    
    // Check for critical sections
    result += "=== Critical Sections ===\n";
    result += handle_execute_command("!critsec");
    
    return result;
}

// Register and Memory Analysis

/**
 * @brief Handles register display requests.
 * 
 * @return Formatted register information
 */
std::string CommandHandlers::handle_show_registers() {
    return handle_execute_command("r");
}

/**
 * @brief Handles call stack display requests.
 * 
 * @return Formatted call stack information
 */
std::string CommandHandlers::handle_show_call_stack() {
    return handle_execute_command("kn");
}

/**
 * @brief Handles stack trace display requests.
 * 
 * @return Formatted stack trace information
 */
std::string CommandHandlers::handle_show_stack_trace() {
    return handle_execute_command("k");
}

// Symbol and Module Analysis

/**
 * @brief Handles symbol loading requests.
 * 
 * @param[in] module_name Name of the module to load symbols for
 * 
 * @return Result of symbol loading operation
 */
std::string CommandHandlers::handle_load_symbols(_In_ std::string_view module_name) {
    std::ostringstream oss;
    oss << ".reload " << module_name;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles symbol information requests.
 * 
 * @param[in] symbol Symbol name to get information for
 * 
 * @return Formatted symbol information
 */
std::string CommandHandlers::handle_show_symbol_info(_In_ std::string_view symbol) {
    std::ostringstream oss;
    oss << "x " << symbol;
    return handle_execute_command(oss.str());
}

// Memory Analysis

/**
 * @brief Handles memory search requests.
 * 
 * @param[in] start_address Starting address for search
 * @param[in] end_address Ending address for search
 * @param[in] pattern Pattern to search for
 * 
 * @return Search results
 */
std::string CommandHandlers::handle_search_memory(
    _In_ uintptr_t start_address, 
    _In_ uintptr_t end_address, 
    _In_ std::string_view pattern) {
    std::ostringstream oss;
    oss << "s -b " << format_hex(start_address) << " " << format_hex(end_address) << " " << pattern;
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles memory region information requests.
 * 
 * @param[in] address Address to get region information for
 * 
 * @return Formatted memory region information
 */
std::string CommandHandlers::handle_show_memory_region(_In_ uintptr_t address) {
    std::ostringstream oss;
    oss << "!address " << format_hex(address);
    return handle_execute_command(oss.str());
}

/**
 * @brief Handles generic command execution for LLM-driven debugging.
 * 
 * This method provides a flexible command routing system that can handle
 * various command formats and route them to appropriate handlers. It
 * includes command normalization, parameter parsing, and fallback to
 * direct execution when no specific handler is available.
 * 
 * @param[in] command The command to execute
 * 
 * @return Result of command execution
 */
std::string CommandHandlers::handle_generic_command(_In_ std::string_view command) {
    LOG_INFO("CommandHandlers", "Processing command: " + std::string(command));
    
    if (!command_executor_) {
        LOG_ERROR("CommandHandlers", "No command executor available");
        return "Error: Internal error";
    }
    
    // Normalize command for routing
    std::string normalized_command = std::string(command);
    std::transform(normalized_command.begin(), normalized_command.end(), 
                   normalized_command.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    LOG_DEBUG("CommandHandlers", "Normalized command: " + normalized_command);
    
    // Remove leading/trailing whitespace
    auto trimmed_command = std::string_view(normalized_command).substr(
        normalized_command.find_first_not_of(" \t\n\r"),
        normalized_command.find_last_not_of(" \t\n\r") - normalized_command.find_first_not_of(" \t\n\r") + 1
    );
    
    LOG_DEBUG("CommandHandlers", "Trimmed command: " + std::string(trimmed_command));
    
    // Try to route to specific handlers first (for better error handling and structured responses)
    LOG_DEBUG("CommandHandlers", "Trying specific handlers");
    auto routed_result = try_route_to_specific_handler(trimmed_command, command);
    if (!routed_result.empty()) {
        LOG_INFO_DETAIL("CommandHandlers", "Routed to specific handler", "Result length: " + std::to_string(routed_result.length()));
        return routed_result;
    }
    
    // If no specific handler, execute directly
    LOG_DEBUG("CommandHandlers", "No specific handler found, executing directly");
    return handle_execute_command(command);
}

/**
 * @brief Attempts to route a command to a specific handler.
 * 
 * This method implements a command routing system that recognizes common
 * WinDbg commands and routes them to appropriate specialized handlers.
 * It provides better error handling and structured responses for known
 * command types while falling back to direct execution for unknown commands.
 * 
 * @param[in] normalized_command Normalized command string for routing
 * @param[in] original_command Original command string for parameter extraction
 * 
 * @return Result from specific handler, or empty string if no handler found
 */
std::string CommandHandlers::try_route_to_specific_handler(
    _In_ std::string_view normalized_command, 
    _In_ std::string_view original_command) {
    // Extract command and parameters
    auto space_pos = normalized_command.find(' ');
    auto cmd = space_pos != std::string::npos ? normalized_command.substr(0, space_pos) : normalized_command;
    auto params = space_pos != std::string::npos ? normalized_command.substr(space_pos + 1) : std::string_view{};
    
    // Route based on command type
    if (cmd == "k" || cmd == "kn" || cmd == "kl" || cmd == "kp" || cmd == "kv") {
        return handle_show_stack_trace();
    }
    
    if (cmd == "~") {
        return handle_list_threads();
    }
    
    if (cmd == "!process" || cmd == "!processes") {
        return handle_list_processes();
    }
    
    if (cmd == "lm" || cmd == "!modules") {
        return handle_list_modules();
    }
    
    if (cmd == "r" || cmd == "registers") {
        return handle_show_registers();
    }
    
    if (cmd == "g" || cmd == "go") {
        return handle_continue_execution();
    }
    
    if (cmd == "p" || cmd == "step") {
        return handle_step_over();
    }
    
    if (cmd == "t" || cmd == "trace") {
        return handle_step_into();
    }
    
    if (cmd == "gu" || cmd == "stepout") {
        return handle_step_out();
    }
    
    if (cmd == "gh") {
        return handle_continue_with_exception_handled();
    }
    
    if (cmd == "gn") {
        return handle_continue_with_exception_not_handled();
    }
    
    if (cmd == "bl" || cmd == "breakpoints") {
        return handle_list_breakpoints();
    }
    
    // Handle breakpoint commands with parameters
    if (cmd == "bp" || cmd == "breakpoint") {
        if (!params.empty()) {
            // Try to parse as address or symbol
            if (params.find("0x") == 0 || params.find_first_of("0123456789abcdefABCDEF") == 0) {
                // Likely an address
                try {
                    uintptr_t addr = std::stoull(std::string(params), nullptr, 16);
                    return handle_set_breakpoint(addr);
                } catch (const std::invalid_argument& e) {
                    LOG_WARNING("CommandHandlers", "Invalid address format: " + std::string(params) + " - " + e.what());
                    return "Error: Invalid address format '" + std::string(params) + "'";
                } catch (const std::out_of_range& e) {
                    LOG_WARNING("CommandHandlers", "Address out of range: " + std::string(params) + " - " + e.what());
                    return "Error: Address '" + std::string(params) + "' is out of range";
                } catch (const std::exception& e) {
                    LOG_WARNING("CommandHandlers", "Exception parsing address: " + std::string(params) + " - " + e.what());
                    return "Error: Failed to parse address '" + std::string(params) + "'";
                } catch (...) {
                    LOG_WARNING("CommandHandlers", "Unknown exception parsing address: " + std::string(params));
                    return "Error: Unknown error parsing address '" + std::string(params) + "'";
                }
            } else {
                // Likely a symbol
                return handle_set_symbol_breakpoint(params);
            }
        }
    }
    
    if (cmd == "bc" || cmd == "clear") {
        if (!params.empty()) {
            try {
                uint32_t bp_id = std::stoul(std::string(params));
                return handle_clear_breakpoint(bp_id);
            } catch (const std::invalid_argument& e) {
                LOG_WARNING("CommandHandlers", "Invalid breakpoint ID format: " + std::string(params) + " - " + e.what());
                return "Error: Invalid breakpoint ID format '" + std::string(params) + "'";
            } catch (const std::out_of_range& e) {
                LOG_WARNING("CommandHandlers", "Breakpoint ID out of range: " + std::string(params) + " - " + e.what());
                return "Error: Breakpoint ID '" + std::string(params) + "' is out of range";
            } catch (const std::exception& e) {
                LOG_WARNING("CommandHandlers", "Exception parsing breakpoint ID: " + std::string(params) + " - " + e.what());
                return "Error: Failed to parse breakpoint ID '" + std::string(params) + "'";
            } catch (...) {
                LOG_WARNING("CommandHandlers", "Unknown exception parsing breakpoint ID: " + std::string(params));
                return "Error: Unknown error parsing breakpoint ID '" + std::string(params) + "'";
            }
        }
    }
    
    if (cmd == "bd" || cmd == "disable") {
        if (!params.empty()) {
            try {
                uint32_t bp_id = std::stoul(std::string(params));
                return handle_disable_breakpoint(bp_id);
            } catch (const std::invalid_argument& e) {
                LOG_WARNING("CommandHandlers", "Invalid breakpoint ID format: " + std::string(params) + " - " + e.what());
                return "Error: Invalid breakpoint ID format '" + std::string(params) + "'";
            } catch (const std::out_of_range& e) {
                LOG_WARNING("CommandHandlers", "Breakpoint ID out of range: " + std::string(params) + " - " + e.what());
                return "Error: Breakpoint ID '" + std::string(params) + "' is out of range";
            } catch (const std::exception& e) {
                LOG_WARNING("CommandHandlers", "Exception parsing breakpoint ID: " + std::string(params) + " - " + e.what());
                return "Error: Failed to parse breakpoint ID '" + std::string(params) + "'";
            } catch (...) {
                LOG_WARNING("CommandHandlers", "Unknown exception parsing breakpoint ID: " + std::string(params));
                return "Error: Unknown error parsing breakpoint ID '" + std::string(params) + "'";
            }
        }
    }
    
    if (cmd == "be" || cmd == "enable") {
        if (!params.empty()) {
            try {
                uint32_t bp_id = std::stoul(std::string(params));
                return handle_enable_breakpoint(bp_id);
            } catch (const std::invalid_argument& e) {
                LOG_WARNING("CommandHandlers", "Invalid breakpoint ID format: " + std::string(params) + " - " + e.what());
                return "Error: Invalid breakpoint ID format '" + std::string(params) + "'";
            } catch (const std::out_of_range& e) {
                LOG_WARNING("CommandHandlers", "Breakpoint ID out of range: " + std::string(params) + " - " + e.what());
                return "Error: Breakpoint ID '" + std::string(params) + "' is out of range";
            } catch (const std::exception& e) {
                LOG_WARNING("CommandHandlers", "Exception parsing breakpoint ID: " + std::string(params) + " - " + e.what());
                return "Error: Failed to parse breakpoint ID '" + std::string(params) + "'";
            } catch (...) {
                LOG_WARNING("CommandHandlers", "Unknown exception parsing breakpoint ID: " + std::string(params));
                return "Error: Unknown error parsing breakpoint ID '" + std::string(params) + "'";
            }
        }
    }
    
    if (cmd == ".attach") {
        if (!params.empty()) {
            try {
                uint32_t pid = std::stoul(std::string(params), nullptr, 16);
                return handle_attach_process(pid);
            } catch (const std::invalid_argument& e) {
                LOG_WARNING("CommandHandlers", "Invalid process ID format: " + std::string(params) + " - " + e.what());
                return "Error: Invalid process ID format '" + std::string(params) + "'";
            } catch (const std::out_of_range& e) {
                LOG_WARNING("CommandHandlers", "Process ID out of range: " + std::string(params) + " - " + e.what());
                return "Error: Process ID '" + std::string(params) + "' is out of range";
            } catch (const std::exception& e) {
                LOG_WARNING("CommandHandlers", "Exception parsing process ID: " + std::string(params) + " - " + e.what());
                return "Error: Failed to parse process ID '" + std::string(params) + "'";
            } catch (...) {
                LOG_WARNING("CommandHandlers", "Unknown exception parsing process ID: " + std::string(params));
                return "Error: Unknown error parsing process ID '" + std::string(params) + "'";
            }
        }
    }
    
    if (cmd == ".detach") {
        return handle_detach_process();
    }
    
    if (cmd == ".create") {
        if (!params.empty()) {
            return handle_create_process(params);
        }
    }
    
    if (cmd == ".restart") {
        return handle_restart_process();
    }
    
    if (cmd == ".kill") {
        return handle_terminate_process();
    }
    
    if (cmd == ".dump") {
        if (!params.empty()) {
            return handle_load_dump(params);
        }
    }
    
    if (cmd == "!analyze") {
        return handle_analyze_crash();
    }
    
    // Memory commands
    if (cmd == "db" || cmd == "dd" || cmd == "dw" || cmd == "dq") {
        // Parse memory display commands
        auto memory_result = try_parse_memory_command(original_command);
        if (!memory_result.empty()) {
            return memory_result;
        }
    }
    
    // Return empty to indicate no specific handler found
    return "";
}

/**
 * @brief Attempts to parse and handle memory display commands.
 * 
 * This method parses memory display commands like "db 0x12345678 L0x100"
 * and routes them to appropriate memory handling methods.
 * 
 * @param[in] command The memory command to parse
 * 
 * @return Result from memory handler, or empty string if parsing fails
 */
std::string CommandHandlers::try_parse_memory_command(_In_ std::string_view command) {
    // Parse commands like "db 0x12345678 L0x100" or "dd 0x12345678"
    std::string cmd_str = std::string(command);
    std::transform(cmd_str.begin(), cmd_str.end(), cmd_str.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    // Extract address and size
    std::regex memory_regex(R"((db|dd|dw|dq)\s+(0x[0-9a-fA-F]+)(?:\s+L(0x[0-9a-fA-F]+))?)");
    std::smatch match;
    
        if (std::regex_search(cmd_str, match, memory_regex)) {
            try {
                uintptr_t address = std::stoull(match[2].str(), nullptr, 16);
                size_t size = 0x100; // Default size
                
                if (match[3].matched) {
                    size = std::stoull(match[3].str(), nullptr, 16);
                }
                
                std::string cmd_type = match[1].str();
                if (cmd_type == "db") {
                    return handle_read_memory(address, size);
                } else if (cmd_type == "dd") {
                    return handle_display_memory(address, size);
                } else if (cmd_type == "dw") {
                    return handle_display_memory(address, size * 2);
                } else if (cmd_type == "dq") {
                    return handle_display_memory(address, size * 8);
                }
            } catch (const std::invalid_argument& e) {
                LOG_WARNING("CommandHandlers", "Invalid memory address/size format: " + std::string(command) + " - " + e.what());
                return "Error: Invalid memory address/size format in command '" + std::string(command) + "'";
            } catch (const std::out_of_range& e) {
                LOG_WARNING("CommandHandlers", "Memory address/size out of range: " + std::string(command) + " - " + e.what());
                return "Error: Memory address/size in command '" + std::string(command) + "' is out of range";
            } catch (const std::exception& e) {
                LOG_WARNING("CommandHandlers", "Exception parsing memory command: " + std::string(command) + " - " + e.what());
                return "Error: Failed to parse memory command '" + std::string(command) + "'";
            } catch (...) {
                LOG_WARNING("CommandHandlers", "Unknown exception parsing memory command: " + std::string(command));
                return "Error: Unknown error parsing memory command '" + std::string(command) + "'";
            }
        }
    
    return "";
}

/**
 * @brief Enhanced command execution with LLM-friendly responses.
 * 
 * This method provides a high-level interface for LLM-driven debugging
 * that handles command execution and provides context-aware responses.
 * It's designed to work well with AI assistants and provides clean,
 * structured output suitable for further processing.
 * 
 * @param[in] command The command to execute
 * @param[in] provide_context Whether to include context information (unused)
 * 
 * @return Clean command execution result suitable for LLM consumption
 */
std::string CommandHandlers::handle_llm_command(
    _In_ std::string_view command, 
    _In_ bool /*provide_context*/) {
    LOG_INFO("LLMHandler", "Starting command execution");
    std::string result = handle_generic_command(command);
    
    LOG_INFO_DETAIL("LLMHandler", "Command execution completed", "Result length: " + std::to_string(result.length()));
    
    // Return result without duplicating command info or context
    // Context is already available via separate status commands
    return result;
}

// Utility functions for better code organization and reusability

/**
 * @brief Formats session status information.
 * 
 * This method creates a human-readable status report including connection
 * state, target information, and current process/thread details.
 * 
 * @return Formatted status string
 */
std::string CommandHandlers::format_session_status() {
    if (!session_manager_) {
        return "Error: Session manager not available";
    }
    
    auto session_state = session_manager_->get_state();
    
    std::string status;
    status += "VibeDbg Status:\n";
    status += "  Connected: " + std::string(session_state.is_connected ? "Yes" : "No") + "\n";
    status += "  Target Running: " + std::string(session_state.is_target_running ? "Yes" : "No") + "\n";
    
        if (session_state.current_process) {
        std::ostringstream oss;
        oss << "  Current Process: " << session_state.current_process->process_name 
            << " (PID: " << session_state.current_process->process_id << ")\n";
        status += oss.str();
    }
    
    if (session_state.current_thread) {
        std::ostringstream oss;
        oss << "  Current Thread: " << session_state.current_thread->state 
            << " (TID: " << session_state.current_thread->thread_id << ")\n";
        status += oss.str();
    }
    
    
    return status;
}

/**
 * @brief Formats session information as JSON.
 * 
 * This method creates a structured JSON representation of the current
 * session state, including all relevant debugging information in a
 * machine-readable format.
 * 
 * @return JSON-formatted session information
 */
std::string CommandHandlers::format_session_json() {
    if (!session_manager_) {
        return "Error: Internal error";
    }
    
    auto session_state = session_manager_->get_state();
    
    nlohmann::json session_json;
    session_json["connected"] = session_state.is_connected;
    session_json["target_running"] = session_state.is_target_running;
    session_json["session_start"] = std::chrono::duration_cast<std::chrono::seconds>(
        session_state.session_start.time_since_epoch()).count();
    
    if (session_state.current_process) {
        session_json["current_process"] = {
            {"process_id", session_state.current_process->process_id},
            {"process_name", session_state.current_process->process_name},
            {"image_path", session_state.current_process->image_path},
            {"is_attached", session_state.current_process->is_attached}
        };
    }
    
    if (session_state.current_thread) {
        session_json["current_thread"] = {
            {"thread_id", session_state.current_thread->thread_id},
            {"process_id", session_state.current_thread->process_id},
            {"is_current", session_state.current_thread->is_current},
            {"state", session_state.current_thread->state}
        };
    }
    
    return session_json.dump(2);
}

