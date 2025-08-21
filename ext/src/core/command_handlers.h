#pragma once

#include "../pch.h"
#include "../../inc/session_manager.h"
#include "../../inc/command_executor.h"

namespace vibedbg::core {

class CommandHandlers {
public:
    explicit CommandHandlers(std::shared_ptr<SessionManager> session_manager,
                           std::shared_ptr<CommandExecutor> command_executor);
    
    // Basic command handlers
    std::string handle_version();
    std::string handle_status();
    std::string handle_help();
    
    // Session management
    std::string handle_session_info();
    std::string handle_mode_detection();
    
    // Process management
    std::string handle_list_processes();
    std::string handle_attach_process(uint32_t process_id);
    std::string handle_detach_process();
    
    // Command execution
    std::string handle_execute_command(std::string_view command);
    
    // Memory operations
    std::string handle_read_memory(uintptr_t address, size_t size);
    std::string handle_display_memory(uintptr_t address, size_t size);
    
    // Module operations
    std::string handle_list_modules();
    std::string handle_module_info(std::string_view module_name);
    
    // Thread operations
    std::string handle_list_threads();
    std::string handle_thread_info(uint32_t thread_id);
    std::string handle_switch_thread(uint32_t thread_id);
    
    // Stack operations
    std::string handle_stack_trace();
    std::string handle_call_stack();

    // Breakpoint Management Commands
    std::string handle_set_breakpoint(uintptr_t address);
    std::string handle_set_symbol_breakpoint(std::string_view symbol);
    std::string handle_set_access_breakpoint(uintptr_t address, std::string_view access_type);
    std::string handle_clear_breakpoint(uint32_t breakpoint_id);
    std::string handle_disable_breakpoint(uint32_t breakpoint_id);
    std::string handle_enable_breakpoint(uint32_t breakpoint_id);
    std::string handle_list_breakpoints();

    // Execution Control Commands
    std::string handle_continue_execution();
    std::string handle_step_over();
    std::string handle_step_into();
    std::string handle_step_out();
    std::string handle_continue_with_exception_handled();
    std::string handle_continue_with_exception_not_handled();

    // Process Management Commands
    std::string handle_create_process(std::string_view process_path);
    std::string handle_restart_process();
    std::string handle_terminate_process();

    // Crash Dump Analysis Commands
    std::string handle_load_dump(std::string_view dump_path);
    std::string handle_analyze_crash();
    std::string handle_analyze_deadlock();

    // Register and Memory Analysis
    std::string handle_show_registers();
    std::string handle_show_call_stack();
    std::string handle_show_stack_trace();

    // Symbol and Module Analysis
    std::string handle_load_symbols(std::string_view module_name);
    std::string handle_show_symbol_info(std::string_view symbol);

    // Memory Analysis
    std::string handle_search_memory(uintptr_t start_address, uintptr_t end_address, std::string_view pattern);
    std::string handle_show_memory_region(uintptr_t address);

    // Generic command execution system for LLM-driven debugging
    std::string handle_generic_command(std::string_view command);
    std::string handle_llm_command(std::string_view command, bool provide_context = true);

private:
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<CommandExecutor> command_executor_;
    
    // Helper methods for generic command routing
    std::string try_route_to_specific_handler(std::string_view normalized_command, std::string_view original_command);
    std::string try_parse_memory_command(std::string_view command);
    
    // Utility functions for better code organization
    std::string format_session_status();
    std::string format_session_json();
    
    // Helper methods
    std::string format_process_list(const std::vector<ProcessInfo>& processes);
    std::string format_thread_list(const std::vector<ThreadInfo>& threads);
    std::string format_memory_display(uintptr_t address, const std::vector<uint8_t>& data);
    std::string format_module_list(const std::vector<std::string>& modules);
};

} // namespace vibedbg::core