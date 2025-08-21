#pragma once

#include "../pch.h"
#include "../../inc/extension.h"
#include "../../inc/session_manager.h"
#include "../../inc/command_executor.h"
#include "../../inc/named_pipe_server.h"
#include "../../inc/error_handling.h"
#include "constants.h"
#include "command_handlers.h"

namespace vibedbg::core {

    /**
     * @brief Main implementation class for the VibeDbg extension.
     * 
     * This class manages the complete lifecycle of the VibeDbg extension,
     * including initialization, shutdown, and coordination between all
     * components. It implements the singleton pattern to ensure only
     * one instance exists throughout the extension's lifetime.
     * 
     * The class coordinates:
     * - WinDbg debugger interfaces
     * - Session management
     * - Command execution
     * - Named pipe communication for MCP integration
     * - Statistics and monitoring
     * 
     * @note This class is thread-safe and handles concurrent access to
     *       its components through proper synchronization.
     */
    class ExtensionImpl {
    public:
        /**
         * @brief Gets the singleton instance of the extension implementation.
         * 
         * @return Reference to the singleton ExtensionImpl instance
         * 
         * @note This method is thread-safe and returns the same instance
         *       across all threads.
         */
        static ExtensionImpl& get_instance();

        // Extension lifecycle
        /**
         * @brief Initializes the extension with the provided debug client.
         * 
         * This method performs complete initialization of the extension,
         * including debugger interfaces, core components, and communication
         * infrastructure. It follows a specific initialization order to
         * ensure proper dependency resolution.
         * 
         * @param[in] debug_client Pointer to the WinDbg debug client interface
         * 
         * @return ExtensionError::None on success, appropriate error code on failure
         * 
         * @note This method is idempotent - calling it multiple times will
         *       return ExtensionError::AlreadyInitialized after the first call.
         */
        ExtensionError initialize(_In_ IDebugClient* debug_client);

        /**
         * @brief Shuts down the extension and cleans up all resources.
         * 
         * This method performs a complete shutdown of the extension in the
         * reverse order of initialization. It ensures all resources are
         * properly cleaned up and the extension is left in a safe state.
         * 
         * @note This method is safe to call multiple times and handles
         *       exceptions gracefully.
         */
        void shutdown();

        /**
         * @brief Checks if the extension is currently initialized.
         * 
         * @return true if initialized, false otherwise
         * 
         * @note This method is thread-safe and provides a consistent view
         *       of the initialization state.
         */
        bool is_initialized() const noexcept { return initialized_.load(); }

        // Debugger interface access
        /**
         * @brief Gets the WinDbg debug client interface.
         * 
         * @return Pointer to the debug client interface, or nullptr if not initialized
         */
        IDebugClient* get_debug_client() const noexcept { return debug_client_; }

        /**
         * @brief Gets the WinDbg debug control interface.
         * 
         * @return Pointer to the debug control interface, or nullptr if not initialized
         */
        IDebugControl* get_debug_control() const noexcept { return debug_control_; }

        /**
         * @brief Gets the WinDbg debug data spaces interface.
         * 
         * @return Pointer to the debug data spaces interface, or nullptr if not initialized
         */
        IDebugDataSpaces* get_debug_data_spaces() const noexcept { return debug_data_spaces_; }

        /**
         * @brief Gets the WinDbg debug registers interface.
         * 
         * @return Pointer to the debug registers interface, or nullptr if not initialized
         */
        IDebugRegisters* get_debug_registers() const noexcept { return debug_registers_; }

        /**
         * @brief Gets the WinDbg debug symbols interface.
         * 
         * @return Pointer to the debug symbols interface, or nullptr if not initialized
         */
        IDebugSymbols* get_debug_symbols() const noexcept { return debug_symbols_; }

        // Component access
        /**
         * @brief Gets the session manager component.
         * 
         * @return Shared pointer to the session manager, or nullptr if not initialized
         */
        std::shared_ptr<SessionManager> get_session_manager() const { return session_manager_; }

        /**
         * @brief Gets the command executor component.
         * 
         * @return Shared pointer to the command executor, or nullptr if not initialized
         */
        std::shared_ptr<CommandExecutor> get_command_executor() const { return command_executor_; }

        /**
         * @brief Gets the named pipe server component.
         * 
         * @return Raw pointer to the pipe server, or nullptr if not initialized
         */
        communication::NamedPipeServer* get_pipe_server() const { return pipe_server_.get(); }

        // Command execution
        /**
         * @brief Executes an extension command through the command executor.
         * 
         * This method provides a safe interface for executing commands through
         * the extension's command executor. It includes proper error handling
         * and statistics tracking.
         * 
         * @param[in] command The command to execute
         * @param[out] error Optional pointer to receive detailed error information
         * 
         * @return The command output as a string, or empty string on failure
         * 
         * @note This method requires the extension to be initialized.
         */
        std::string execute_extension_command(
            _In_ std::string_view command, 
            _Out_opt_ ExtensionError* error = nullptr);

        // Statistics
        /**
         * @brief Statistics structure for tracking extension performance and usage.
         */
        struct Stats {
            std::chrono::time_point<std::chrono::steady_clock> init_time;  ///< Extension initialization time
            uint64_t total_connections = 0;      ///< Total number of MCP connections
            uint64_t total_commands = 0;         ///< Total number of commands executed
            uint64_t successful_commands = 0;    ///< Number of successfully executed commands
            uint64_t failed_commands = 0;        ///< Number of failed command executions
        };

        /**
         * @brief Gets current extension statistics.
         * 
         * @return Copy of the current statistics
         * 
         * @note This method is thread-safe and provides a consistent snapshot
         *       of the statistics at the time of the call.
         */
        Stats get_stats() const;

    private:
        /**
         * @brief Private constructor for singleton pattern.
         */
        ExtensionImpl() = default;

        /**
         * @brief Private destructor for singleton pattern.
         */
        ~ExtensionImpl() = default;

        /**
         * @brief Deleted copy constructor to prevent copying.
         */
        ExtensionImpl(const ExtensionImpl&) = delete;

        /**
         * @brief Deleted assignment operator to prevent assignment.
         */
        ExtensionImpl& operator=(const ExtensionImpl&) = delete;

        std::atomic<bool> initialized_{ false };  ///< Thread-safe initialization flag

        // WinDbg interfaces
        IDebugClient* debug_client_{ nullptr };       ///< WinDbg debug client interface
        IDebugControl* debug_control_{ nullptr };     ///< WinDbg debug control interface
        IDebugDataSpaces* debug_data_spaces_{ nullptr }; ///< WinDbg debug data spaces interface
        IDebugRegisters* debug_registers_{ nullptr }; ///< WinDbg debug registers interface
        IDebugSymbols* debug_symbols_{ nullptr };     ///< WinDbg debug symbols interface

        // Core components
        std::shared_ptr<SessionManager> session_manager_;           ///< Session management component
        std::shared_ptr<CommandExecutor> command_executor_;         ///< Command execution component
        std::unique_ptr<communication::NamedPipeServer> pipe_server_; ///< Named pipe communication server
        std::unique_ptr<CommandHandlers> command_handlers_;         ///< Command routing and handling

        // Statistics
        mutable std::mutex stats_mutex_;  ///< Mutex for protecting statistics access
        Stats stats_;                     ///< Current extension statistics

        // Initialization helpers
        /**
         * @brief Initializes WinDbg debugger interfaces.
         * 
         * @return ExtensionError::None on success, appropriate error code on failure
         */
        ExtensionError initialize_debugger_interfaces();

        /**
         * @brief Initializes core extension components.
         * 
         * @return ExtensionError::None on success, appropriate error code on failure
         */
        ExtensionError initialize_core_components();

        /**
         * @brief Initializes communication infrastructure.
         * 
         * @return ExtensionError::None on success, appropriate error code on failure
         */
        ExtensionError initialize_communication();

        // Message handling
        /**
         * @brief Handles MCP command requests from the named pipe server.
         * 
         * @param[in] request The command request to process
         * @param[out] error Optional pointer to receive error information
         * 
         * @return CommandResponse containing the result of command execution
         */
        communication::CommandResponse handle_mcp_command(
            _In_ const communication::CommandRequest& request, 
            _Out_opt_ communication::ErrorCode* error = nullptr);

        // Cleanup
        /**
         * @brief Cleans up WinDbg debugger interfaces.
         */
        void cleanup_interfaces();

        /**
         * @brief Cleans up core extension components.
         */
        void cleanup_components();

        /**
         * @brief Cleans up communication infrastructure.
         */
        void cleanup_communication();

        
    };

} // namespace vibedbg::core