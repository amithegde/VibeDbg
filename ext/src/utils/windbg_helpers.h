#pragma once

#include "../pch.h"
#include "../../inc/session_manager.h"

namespace vibedbg::utils {

class WinDbgHelpers {
public:
    // Command execution helpers
    static std::string execute_command(std::string_view command, HRESULT* hr = nullptr);
    static std::string execute_command_with_timeout(
        std::string_view command, 
        std::chrono::milliseconds timeout,
        HRESULT* hr = nullptr);
    
    // Output capture
    static std::string capture_command_output(std::string_view command, HRESULT* hr = nullptr);
    
    // Debugging state helpers
    static bool is_user_mode_debugging();
    static bool is_live_debugging();
    
    // Process and thread helpers
    static uint32_t get_current_process_id(HRESULT* hr = nullptr);
    static uint32_t get_current_thread_id(HRESULT* hr = nullptr);
    static std::string get_current_process_name(HRESULT* hr = nullptr);
    
    // Memory helpers
    static std::vector<uint8_t> read_memory(
        uintptr_t address, 
        size_t size,
        HRESULT* hr = nullptr);
    static HRESULT write_memory(
        uintptr_t address, 
        const std::vector<uint8_t>& data);
    
    // Symbol helpers
    static uintptr_t get_symbol_address(std::string_view symbol, HRESULT* hr = nullptr);
    static std::string get_symbol_name(uintptr_t address, HRESULT* hr = nullptr);
    
    // Module helpers
    static std::vector<std::string> get_loaded_modules(HRESULT* hr = nullptr);
    static uintptr_t get_module_base(std::string_view module_name, HRESULT* hr = nullptr);
    
    // Error handling
    static std::string format_windbg_error(HRESULT hr);
    static std::string format_last_error();
    
    // String utilities
    static std::string trim_whitespace(const std::string& str);
    static std::vector<std::string> split_lines(const std::string& str);
    static std::string join_lines(const std::vector<std::string>& lines);

private:
    // Internal helpers
    static IDebugControl* get_debug_control();
    static IDebugDataSpaces* get_debug_data_spaces();
    static IDebugSymbols* get_debug_symbols();
    static IDebugRegisters* get_debug_registers();
    static IDebugClient* get_debug_client();
};


} // namespace vibedbg::utils