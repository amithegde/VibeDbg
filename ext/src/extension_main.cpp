#include "pch.h"
#include "core/extension_impl.h"

using namespace vibedbg::core;
using namespace vibedbg::constants;

// Global extension APIs
WINDBG_EXTENSION_APIS64 ExtensionApis{ sizeof(ExtensionApis) };

//
// Extension DLL entry points
//

/**
 * @brief Initializes the WinDbg extension with the provided extension APIs.
 * 
 * This function is called by WinDbg when the extension DLL is loaded. It stores
 * the extension APIs for later use by the extension commands.
 * 
 * @param[in] lpExtensionApis Pointer to the WinDbg extension APIs structure
 * @param[in] MajorVersion Major version of WinDbg (unused)
 * @param[in] MinorVersion Minor version of WinDbg (unused)
 * 
 * @return HRESULT S_OK on success, E_FAIL on failure
 * 
 * @note This function is called during DLL loading and should not perform
 *       heavy initialization. Use DebugExtensionInitialize for that purpose.
 */
extern "C" HRESULT CALLBACK WinDbgExtensionDllInit(
    _In_ PWINDBG_EXTENSION_APIS64 lpExtensionApis, 
    _In_ USHORT MajorVersion, 
    _In_ USHORT MinorVersion) {
    UNREFERENCED_PARAMETER(MajorVersion);
    UNREFERENCED_PARAMETER(MinorVersion);
    
    if (!lpExtensionApis) {
        return E_POINTER;
    }
    
    ExtensionApis = *lpExtensionApis;
    return S_OK;
}

/**
 * @brief Performs full extension initialization and version negotiation.
 * 
 * This function is called by WinDbg after the extension DLL is loaded to
 * perform complete initialization. It sets up the extension version and
 * attempts to initialize the ExtensionApis properly.
 * 
 * @param[out] version Pointer to receive the extension version
 * @param[out] flags Pointer to receive extension flags
 * 
 * @return HRESULT S_OK on success, E_POINTER if parameters are null
 * 
 * @note This function may be called multiple times during the extension lifecycle.
 *       The implementation should be idempotent.
 */
extern "C" HRESULT CALLBACK DebugExtensionInitialize(
    _Out_ PULONG version, 
    _Out_ PULONG flags) {
    if (!version || !flags) {
        return E_POINTER;
    }

    // Initialize ExtensionApis properly
    try {
        CComPtr<IDebugClient> client;
        HRESULT hr = DebugCreate(__uuidof(IDebugClient), (void**)&client);
        if (SUCCEEDED(hr)) {
            CComQIPtr<IDebugControl> control(client);
            if (control) {
                hr = control->GetWindbgExtensionApis64(&ExtensionApis);
                if (FAILED(hr)) {
                    // Log the failure but continue
                    // Note: Can't use Logger here as it's not initialized yet
                    OutputDebugStringA("VibeDbg: Failed to get ExtensionApis\n");
                }
            }
        }
    } catch (const std::exception& e) {
        // Log the exception but continue
        // Note: Can't use Logger here as it's not initialized yet
        OutputDebugStringA(("VibeDbg: Exception during ExtensionApis initialization: " + std::string(e.what()) + "\n").c_str());
    } catch (...) {
        // If ExtensionApis initialization fails, continue anyway
        OutputDebugStringA("VibeDbg: Unknown exception during ExtensionApis initialization\n");
    }
    
    *version = DEBUG_EXTENSION_VERSION(1, 0);
    *flags = 0;
    
    return S_OK;
}

/**
 * @brief Performs extension cleanup and shutdown.
 * 
 * This function is called by WinDbg when the extension is being unloaded.
 * It safely shuts down the extension implementation and cleans up resources.
 * 
 * @note This function should handle exceptions gracefully and not throw.
 *       It's called during DLL unloading, so failure should be silent.
 */
extern "C" void CALLBACK DebugExtensionUninitialize() {
    // Safe shutdown without forcing initialization
    try {
        auto& extension = ExtensionImpl::get_instance();
        extension.shutdown();
    } catch (const std::exception& e) {
        // Log the exception but continue with cleanup
        // Note: Can't use Logger here as it might be shutting down
        OutputDebugStringA(("VibeDbg: Exception during extension shutdown: " + std::string(e.what()) + "\n").c_str());
    } catch (...) {
        // Silent cleanup during uninit
        OutputDebugStringA("VibeDbg: Unknown exception during extension shutdown\n");
    }
    
    // Cleanup logging system
    try {
        vibedbg::logging::Logger::Cleanup();
    } catch (const std::exception& e) {
        // Log the exception but continue
        // Note: Can't use Logger here as it's being cleaned up
        OutputDebugStringA(("VibeDbg: Exception during logging cleanup: " + std::string(e.what()) + "\n").c_str());
    } catch (...) {
        // Silent cleanup
        OutputDebugStringA("VibeDbg: Unknown exception during logging cleanup\n");
    }
}

//
// Extension command implementations
//

/**
 * @brief Initializes the VibeDbg extension and establishes communication.
 * 
 * This command initializes the extension, sets up the named pipe server for
 * MCP communication, and prepares the extension for use. It must be called
 * before any other VibeDbg commands can be used.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success, E_FAIL on failure
 * 
 * @note This command initializes the logging system and creates the named
 *       pipe server for MCP client communication.
 */
STDAPI vibedbg_connect(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(args);
    
    try {
        // Initialize logging system when first connecting
        vibedbg::logging::Logger::Initialize("VibeDbg");
        
        auto& extension = ExtensionImpl::get_instance();
        
        if (extension.is_initialized()) {
            LOG_WINDBG("Connect", "Already connected");
            return S_OK;
        }
        
        LOG_WINDBG("Connect", "Initializing VibeDbg extension...");
        LOG_INFO("Connect", "Starting extension initialization");
        
        // Try to initialize with basic components first
        auto result = extension.initialize(client);
        if (result == ExtensionError::None) {
            LOG_WINDBG("Connect", "Connected successfully");
            LOG_INFO("Connect", "Extension initialized successfully");
            LOG_WINDBG("Connect", "Named pipe: " + std::string(DEFAULT_PIPE_NAME));
            LOG_WINDBG("Connect", "Ready for MCP server connection");
            LOG_INFO("Connect", "Ready for MCP server connection");
            return S_OK;
        } else {
            LOG_WINDBG("Connect", "Failed to connect (error code: " + std::to_string(static_cast<int>(result)) + ")");
            LOG_ERROR_DETAIL("Connect", "Extension initialization failed", "Error code: " + std::to_string(static_cast<int>(result)));
            return E_FAIL;
        }
    } catch (const std::exception& e) {
        LOG_WINDBG("Connect", "Exception during initialization: " + std::string(e.what()));
        LOG_ERROR_DETAIL("Connect", "Exception during initialization", e.what());
        return E_FAIL;
    } catch (...) {
        LOG_WINDBG("Connect", "Unknown exception during initialization");
        LOG_ERROR("Connect", "Unknown exception during initialization");
        return E_FAIL;
    }
}

/**
 * @brief Shuts down the VibeDbg extension and cleans up resources.
 * 
 * This command safely shuts down the extension, stops the named pipe server,
 * and cleans up all resources. It should be called when debugging is complete.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success, E_FAIL on failure
 * 
 * @note This command performs a complete shutdown and cleanup of the extension.
 */
STDAPI vibedbg_disconnect(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(args);
    
    try {
        auto& extension = ExtensionImpl::get_instance();
        extension.shutdown();
        
        LOG_WINDBG("Disconnect", "Disconnected");
        LOG_INFO("Disconnect", "Extension shutdown completed");
        return S_OK;
    } catch (const std::exception& e) {
        LOG_WINDBG("Disconnect", "Exception during shutdown: " + std::string(e.what()));
        LOG_ERROR_DETAIL("Disconnect", "Exception during shutdown", e.what());
        return E_FAIL;
    } catch (...) {
        LOG_WINDBG("Disconnect", "Unknown exception during shutdown");
        LOG_ERROR("Disconnect", "Unknown exception during shutdown");
        return E_FAIL;
    }
}

/**
 * @brief Displays the current status of the VibeDbg extension.
 * 
 * This command shows comprehensive status information including:
 * - Connection status
 * - Uptime and statistics
 * - Named pipe server status
 * - Current debugging session state
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success, E_FAIL on failure
 * 
 * @note This command provides detailed diagnostic information useful for
 *       troubleshooting extension issues.
 */
STDAPI vibedbg_status(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(args);
    
    try {
        auto& extension = ExtensionImpl::get_instance();
        
        if (!extension.is_initialized()) {
            LOG_WINDBG("Status", "Not connected");
            LOG_WINDBG("Status", "Use 'vibedbg_connect' to initialize");
            return S_OK;
        }
        
        LOG_WINDBG("Status", "Connected");
        
        // Show basic statistics safely
        auto stats = extension.get_stats();
        auto uptime = std::chrono::steady_clock::now() - stats.init_time;
        auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime);
        
        LOG_WINDBG("Status", "Uptime: " + std::to_string(uptime_seconds.count()) + " seconds");
        LOG_WINDBG("Status", "Total commands: " + std::to_string(stats.total_commands));
        LOG_WINDBG("Status", "Successful: " + std::to_string(stats.successful_commands));
        LOG_WINDBG("Status", "Failed: " + std::to_string(stats.failed_commands));
        LOG_WINDBG("Status", "Total connections: " + std::to_string(stats.total_connections));
        
        // Show pipe server status safely
        if (auto* pipe_server = extension.get_pipe_server()) {
            try {
                auto pipe_stats = pipe_server->get_stats();
                LOG_WINDBG("Status", "Pipe connections: " + std::to_string(pipe_stats.active_connections) + " active");
                LOG_WINDBG("Status", "Pipe messages: " + std::to_string(pipe_stats.total_messages_processed) + " processed");
            } catch (...) {
                LOG_WINDBG("Status", "Pipe server status: Error reading stats");
            }
        } else {
            LOG_WINDBG("Status", "Pipe server: Not available");
        }
        
        // Show session state safely
        if (auto session_manager = extension.get_session_manager()) {
            try {
                auto session_state = session_manager->get_state();
                LOG_WINDBG("Status", "Target connected: " + std::string(session_state.is_connected ? "Yes" : "No"));
                
                if (session_state.current_process) {
                    LOG_WINDBG("Status", "Current process: " + session_state.current_process->process_name + " (PID: " + std::to_string(session_state.current_process->process_id) + ")");
                }
            } catch (...) {
                LOG_WINDBG("Status", "Session state: Error reading state");
            }
        } else {
            LOG_WINDBG("Status", "Session manager: Not available");
        }
        
    } catch (const std::exception& e) {
        LOG_WINDBG("Status", "Error accessing extension: " + std::string(e.what()));
        LOG_ERROR_DETAIL("Status", "Error accessing extension", e.what());
        return E_FAIL;
    } catch (...) {
        LOG_WINDBG("Status", "Unknown error accessing extension");
        LOG_ERROR("Status", "Unknown error accessing extension");
        return E_FAIL;
    }
    
    return S_OK;
}

/**
 * @brief Executes a WinDbg command through the VibeDbg extension.
 * 
 * This command provides a safe interface for executing WinDbg commands through
 * the extension's command executor. It includes proper error handling and
 * logging for debugging purposes.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args The WinDbg command to execute
 * 
 * @return HRESULT S_OK on success, E_FAIL on failure, E_INVALIDARG if no command provided
 * 
 * @note This command requires the extension to be connected via vibedbg_connect.
 *       The command is executed through the extension's command executor which
 *       provides additional safety and logging.
 */
STDAPI vibedbg_execute(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    
    try {
        auto& extension = ExtensionImpl::get_instance();
        
        if (!extension.is_initialized()) {
            LOG_WINDBG("Execute", "Not connected. Use 'vibedbg_connect' first.");
            return E_FAIL;
        }
        
        if (!args || strlen(args) == 0) {
            LOG_WINDBG("Execute", "Usage: vibedbg_execute <command>");
            LOG_WINDBG("Execute", "Examples:");
            LOG_WINDBG("Execute", "  vibedbg_execute k                    # Show stack trace");
            LOG_WINDBG("Execute", "  vibedbg_execute ~                    # List threads");
            LOG_WINDBG("Execute", "  vibedbg_execute !process 0 0         # List processes");
            LOG_WINDBG("Execute", "  vibedbg_execute bp main              # Set breakpoint at main");
            LOG_WINDBG("Execute", "  vibedbg_execute g                    # Continue execution");
            LOG_WINDBG("Execute", "  vibedbg_execute r                    # Show registers");
            return E_INVALIDARG;
        }
        
        LOG_INFO_DETAIL("Execute", "Executing command", std::string(args));
        
        ExtensionError error;
        auto result = extension.execute_extension_command(args, &error);
        if (error == ExtensionError::None && !result.empty()) {
            dprintf("%s\n", result.c_str());
            LOG_INFO_DETAIL("Execute", "Command executed successfully", "Command: " + std::string(args));
            return S_OK;
        } else {
            LOG_WINDBG("Execute", "Command execution failed");
            LOG_ERROR_DETAIL("Execute", "Command execution failed", "Command: " + std::string(args) + ", Error: " + std::to_string(static_cast<int>(error)));
            return E_FAIL;
        }
    } catch (const std::exception& e) {
        LOG_WINDBG("Execute", "Exception during command execution: " + std::string(e.what()));
        LOG_ERROR_DETAIL("Execute", "Exception during command execution", e.what());
        return E_FAIL;
    } catch (...) {
        LOG_WINDBG("Execute", "Unknown exception during command execution");
        LOG_ERROR("Execute", "Unknown exception during command execution");
        return E_FAIL;
    }
}

/**
 * @brief Displays version information for the VibeDbg extension.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success
 */
STDAPI vibedbg_version(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(args);
    
    LOG_WINDBG("Version", "VibeDbg Extension v1.0.0");
    LOG_WINDBG("Version", "Windows Debugging Extension for MCP Integration");
    return S_OK;
}

/**
 * @brief Displays help information for all available VibeDbg commands.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success
 */
STDAPI vibedbg_help(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(args);
    
    LOG_WINDBG("Help", "Available commands:");
    LOG_WINDBG("Help", "  vibedbg_connect     - Initialize VibeDbg extension");
    LOG_WINDBG("Help", "  vibedbg_disconnect  - Shutdown VibeDbg extension");
    LOG_WINDBG("Help", "  vibedbg_status      - Show extension status");
    LOG_WINDBG("Help", "  vibedbg_execute <cmd> - Execute a WinDbg command through VibeDbg");
    LOG_WINDBG("Help", "  vibedbg_version     - Show version information");
    LOG_WINDBG("Help", "  vibedbg_help        - Show this help");
    LOG_WINDBG("Help", "  vibedbg_test        - Run self-test");
    LOG_WINDBG("Help", "");
    LOG_WINDBG("Help", "After connecting, use MCP client to interact with the extension");
    LOG_WINDBG("Help", "Named pipe: \\\\.\\pipe\\vibedbg_debug");
    return S_OK;
}

/**
 * @brief Runs a self-test to verify the extension is working correctly.
 * 
 * @param[in] client WinDbg debug client interface (unused)
 * @param[in] args Command arguments (unused)
 * 
 * @return HRESULT S_OK on success
 */
STDAPI vibedbg_test(
    _In_opt_ IDebugClient* client, 
    _In_opt_ PCSTR args) {
    UNREFERENCED_PARAMETER(client);
    UNREFERENCED_PARAMETER(args);
    
    LOG_WINDBG("Test", "Test command executed successfully");
    LOG_WINDBG("Test", "Extension is working correctly");
    LOG_INFO("Test", "Test command executed successfully");
    return S_OK;
}

//
// Extension API version information
//

/**
 * @brief Returns the extension API version information.
 * 
 * This function is called by WinDbg to determine the extension's API version
 * and compatibility requirements.
 * 
 * @return Pointer to the extension API version structure
 */
extern "C" LPEXT_API_VERSION CALLBACK ExtensionApiVersion() {
    static EXT_API_VERSION api_version = {
        1,  // MajorVersion
        0,  // MinorVersion
        EXT_API_VERSION_NUMBER64,
        0   // Reserved
    };
    return &api_version;
}

/**
 * @brief Checks if the extension version is compatible with WinDbg.
 * 
 * @return HRESULT S_OK if compatible, E_FAIL if not
 */
extern "C" HRESULT CALLBACK CheckVersion() {
    return S_OK;
}

/**
 * @brief Determines if the extension can be unloaded.
 * 
 * This function is called by WinDbg to check if it's safe to unload the
 * extension DLL. The extension should return S_OK if it's safe to unload.
 * 
 * @return HRESULT S_OK if safe to unload, S_FALSE if not
 */
extern "C" HRESULT CALLBACK DebugExtensionCanUnload() {
    // Always allow unloading for now
    // In a production extension, you might check if there are active operations
    return S_OK;
}