#pragma once

#include <Windows.h>
#include <DbgEng.h>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include "named_pipe_server.h"
#include "message_protocol.h"
#include "session_manager.h"
#include "command_executor.h"

namespace vibedbg::core {

enum class ExtensionError : uint32_t {
    None = 0,
    InitializationFailed = 1,
    DebuggerInterfaceError = 2,
    CommunicationSetupFailed = 3,
    AlreadyInitialized = 4,
    NotInitialized = 5,
    ShutdownFailed = 6,
    InternalError = 7
};

struct ExtensionConfig {
    std::string pipe_name = R"(\\.\pipe\vibedbg_debug)";
    uint32_t max_connections = 10;
    uint32_t worker_threads = 2;
    bool enable_logging = true;
    std::string log_file_path;
    bool enable_heartbeat = true;
    std::chrono::milliseconds heartbeat_interval{10000};
    bool auto_detect_mode = true;
    bool validate_commands = true;
};

class WindbgExtension {
public:
    static WindbgExtension& get_instance();
    
    // Extension lifecycle
    ExtensionError initialize(IDebugClient* debug_client, const ExtensionConfig& config = {});
    void shutdown();
    bool is_initialized() const noexcept { return initialized_.load(); }

    // Debugger interface access
    IDebugClient* get_debug_client() const noexcept { return debug_client_; }
    IDebugControl* get_debug_control() const noexcept { return debug_control_; }
    IDebugDataSpaces* get_debug_data_spaces() const noexcept { return debug_data_spaces_; }
    IDebugRegisters* get_debug_registers() const noexcept { return debug_registers_; }
    IDebugSymbols* get_debug_symbols() const noexcept { return debug_symbols_; }

    // Session and command access
    std::shared_ptr<SessionManager> get_session_manager() const { return session_manager_; }
    std::shared_ptr<CommandExecutor> get_command_executor() const { return command_executor_; }

    // Direct command execution (for WinDbg command exports)
    std::string execute_extension_command(std::string_view command, ExtensionError* error = nullptr);

    // Statistics and monitoring
    struct ExtensionStats {
        std::chrono::time_point<std::chrono::steady_clock> init_time;
        std::chrono::milliseconds uptime{0};
        uint64_t total_mcp_connections = 0;
        uint64_t total_commands_processed = 0;
        communication::NamedPipeServer::ServerStats pipe_stats;
        CommandExecutor::ExecutorStats executor_stats;
    };

    ExtensionStats get_stats() const;

private:
    WindbgExtension() = default;
    ~WindbgExtension() = default;
    WindbgExtension(const WindbgExtension&) = delete;
    WindbgExtension& operator=(const WindbgExtension&) = delete;

    std::atomic<bool> initialized_{false};
    ExtensionConfig config_;

    // WinDbg interfaces
    IDebugClient* debug_client_{nullptr};
    IDebugControl* debug_control_{nullptr};
    IDebugDataSpaces* debug_data_spaces_{nullptr};
    IDebugRegisters* debug_registers_{nullptr};
    IDebugSymbols* debug_symbols_{nullptr};

    // Core components
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<CommandExecutor> command_executor_;
    std::unique_ptr<communication::NamedPipeServer> pipe_server_;

    // Statistics
    mutable std::mutex stats_mutex_;
    ExtensionStats stats_;

    // Initialization helpers
    ExtensionError initialize_debugger_interfaces();
    ExtensionError initialize_core_components();
    ExtensionError initialize_communication();
    void setup_message_handler();

    // Message handling
    communication::CommandResponse handle_mcp_command(const communication::CommandRequest& request, communication::ErrorCode* error = nullptr);

    // Command routing
    communication::CommandResponse route_command(const communication::CommandRequest& request, communication::ErrorCode* error = nullptr);

    // Cleanup helpers
    void cleanup_debugger_interfaces();
    void cleanup_core_components();
    void cleanup_communication();

    // Error handling and logging
    void log_error(const std::string& message, ExtensionError error);
    void log_info(const std::string& message);
    void log_debug(const std::string& message);
};

} // namespace vibedbg::core

// C-style API for WinDbg extension exports
extern "C" {
    // Extension initialization/cleanup
    HRESULT CALLBACK DebugExtensionInitialize(PULONG version, PULONG flags);
    void CALLBACK DebugExtensionUninitialize();
    
    // Extension commands that can be called from WinDbg
    HRESULT CALLBACK vibedbg_status(IDebugClient* client, PCSTR args);
    HRESULT CALLBACK vibedbg_connect(IDebugClient* client, PCSTR args);
    HRESULT CALLBACK vibedbg_execute(IDebugClient* client, PCSTR args);
    HRESULT CALLBACK vibedbg_help(IDebugClient* client, PCSTR args);
    
    // Extension information
    LPEXT_API_VERSION CALLBACK ExtensionApiVersion();
    HRESULT CALLBACK CheckVersion();
}

// Helper macros for WinDbg extension development
#define VIBEDBG_EXTENSION_VERSION_MAJOR 1
#define VIBEDBG_EXTENSION_VERSION_MINOR 0
#define VIBEDBG_EXTENSION_VERSION_PATCH 0

#define VIBEDBG_CHECK_INITIALIZED() \
    do { \
        if (!vibedbg::core::WindbgExtension::get_instance().is_initialized()) { \
            dprintf("VibeDbg extension is not initialized. Run 'vibedbg_connect' first.\n"); \
            return E_FAIL; \
        } \
    } while(0)

#define VIBEDBG_SAFE_EXECUTE(expr) \
    do { \
        try { \
            auto result = (expr); \
            if (!result) { \
                dprintf("VibeDbg: Operation failed\n"); \
                return E_FAIL; \
            } \
        } catch (const std::exception& e) { \
            dprintf("VibeDbg: Exception: %s\n", e.what()); \
            return E_FAIL; \
        } catch (...) { \
            dprintf("VibeDbg: Unknown exception\n"); \
            return E_FAIL; \
        } \
    } while(0)