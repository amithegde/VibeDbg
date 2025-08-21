#include "pch.h"
#include "extension_impl.h"
#include "../../inc/error_handling.h"

using namespace vibedbg::core;
using namespace vibedbg::communication;

/**
 * @brief Gets the singleton instance of the extension implementation.
 * 
 * This function implements the thread-safe singleton pattern using
 * C++11's guarantee that static local variable initialization is
 * thread-safe.
 * 
 * @return Reference to the singleton ExtensionImpl instance
 */
ExtensionImpl& ExtensionImpl::get_instance() {
    static ExtensionImpl instance;
    return instance;
}

/**
 * @brief Initializes the extension with the provided debug client.
 * 
 * This method performs complete initialization of the extension in a specific
 * order to ensure proper dependency resolution:
 * 1. Debugger interfaces initialization
 * 2. Core components initialization  
 * 3. Communication infrastructure initialization
 * 
 * If any step fails, the method performs cleanup of already initialized
 * components and returns an appropriate error code.
 * 
 * @param[in] debug_client Pointer to the WinDbg debug client interface
 * 
 * @return ExtensionError::None on success, appropriate error code on failure
 */
ExtensionError ExtensionImpl::initialize(_In_ IDebugClient* debug_client) {
    if (initialized_.load()) {
        return ExtensionError::AlreadyInitialized;
    }
    
    if (!debug_client) {
        return ExtensionError::InitializationFailed;
    }
    
    debug_client_ = debug_client;
    
    // Initialize debugger interfaces
    LOG_WINDBG("Extension", "Initializing debugger interfaces...");
    LOG_INFO("Extension", "Initializing debugger interfaces");
    ExtensionError result = initialize_debugger_interfaces();
    if (result != ExtensionError::None) {
        LOG_WINDBG("Extension", "Failed to initialize debugger interfaces");
        LOG_ERROR_DETAIL("Extension", "Failed to initialize debugger interfaces", "Error code: " + std::to_string(static_cast<int>(result)));
        cleanup_interfaces();
        return result;
    }
    LOG_WINDBG("Extension", "Debugger interfaces initialized");
    LOG_INFO("Extension", "Debugger interfaces initialized");
    
    // Initialize core components
    LOG_WINDBG("Extension", "Initializing core components...");
    result = initialize_core_components();
    if (result != ExtensionError::None) {
        LOG_WINDBG("Extension", "Failed to initialize core components");
        LOG_ERROR_DETAIL("Extension", "Failed to initialize core components", "Error code: " + std::to_string(static_cast<int>(result)));
        cleanup_components();
        cleanup_interfaces();
        return result;
    }
    LOG_WINDBG("Extension", "Core components initialized");
    
    // Initialize communication
    LOG_WINDBG("Extension", "Initializing communication...");
    result = initialize_communication();
    if (result != ExtensionError::None) {
        LOG_WINDBG("Extension", "Failed to initialize communication");
        LOG_ERROR_DETAIL("Extension", "Failed to initialize communication", "Error code: " + std::to_string(static_cast<int>(result)));
        cleanup_communication();
        cleanup_components();
        cleanup_interfaces();
        return result;
    }
    LOG_WINDBG("Extension", "Communication initialized");
    
    stats_.init_time = std::chrono::steady_clock::now();
    initialized_.store(true);
    
    LOG_INFO("Extension", "VibeDbg extension initialized successfully");
    return ExtensionError::None;
}

/**
 * @brief Shuts down the extension and cleans up all resources.
 * 
 * This method performs a complete shutdown of the extension in the reverse
 * order of initialization to ensure proper cleanup:
 * 1. Communication infrastructure cleanup
 * 2. Core components cleanup
 * 3. Debugger interfaces cleanup
 * 
 * The method is safe to call multiple times and handles exceptions gracefully.
 */
void ExtensionImpl::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    LOG_INFO("Extension", "Shutting down VibeDbg extension");
    
    cleanup_communication();
    cleanup_components();
    cleanup_interfaces();
    
    initialized_.store(false);
}

/**
 * @brief Executes an extension command through the command executor.
 * 
 * This method provides a safe interface for executing commands through the
 * extension's command executor. It includes proper error handling, statistics
 * tracking, and logging for debugging purposes.
 * 
 * @param[in] command The command to execute
 * @param[out] error Optional pointer to receive detailed error information
 * 
 * @return The command output as a string, or empty string on failure
 */
std::string ExtensionImpl::execute_extension_command(
    _In_ std::string_view command, 
    _Out_opt_ ExtensionError* error) {
    if (!initialized_.load()) {
        if (error) *error = ExtensionError::NotInitialized;
        return "";
    }
    
    if (!command_executor_) {
        if (error) *error = ExtensionError::InternalError;
        return "";
    }
    
    try {
        ExecutionError exec_error;
        auto result = command_executor_->execute_command(command, {}, &exec_error);
        
        if (exec_error == ExecutionError::None && result.success) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_commands++;
            stats_.successful_commands++;
            
            if (error) *error = ExtensionError::None;
            return result.output;
        } else {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_commands++;
            stats_.failed_commands++;
            
            if (error) *error = ExtensionError::InternalError;
            return "";
        }
    } catch (...) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_commands++;
        stats_.failed_commands++;
        
        if (error) *error = ExtensionError::InternalError;
        return "";
    }
}

/**
 * @brief Gets current extension statistics.
 * 
 * This method provides a thread-safe way to access the current extension
 * statistics. It returns a copy of the statistics to ensure consistency
 * even if the statistics are updated concurrently.
 * 
 * @return Copy of the current statistics
 */
ExtensionImpl::Stats ExtensionImpl::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// Private implementation methods

/**
 * @brief Initializes WinDbg debugger interfaces.
 * 
 * This method queries the debug client for all required WinDbg interfaces:
 * - IDebugControl for command execution and control
 * - IDebugDataSpaces for memory access
 * - IDebugRegisters for register access
 * - IDebugSymbols for symbol resolution
 * 
 * @return ExtensionError::None on success, ExtensionError::DebuggerInterfaceError on failure
 */
ExtensionError ExtensionImpl::initialize_debugger_interfaces() {
    HRESULT hr;
    
    // Get IDebugControl interface
    hr = debug_client_->QueryInterface(__uuidof(IDebugControl), 
                                      reinterpret_cast<void**>(&debug_control_));
    if (FAILED(hr)) {
        LOG_ERROR("Extension", "Failed to get IDebugControl interface");
        return ExtensionError::DebuggerInterfaceError;
    }
    
    // Get IDebugDataSpaces interface
    hr = debug_client_->QueryInterface(__uuidof(IDebugDataSpaces),
                                      reinterpret_cast<void**>(&debug_data_spaces_));
    if (FAILED(hr)) {
        LOG_ERROR("Extension", "Failed to get IDebugDataSpaces interface");
        return ExtensionError::DebuggerInterfaceError;
    }
    
    // Get IDebugRegisters interface
    hr = debug_client_->QueryInterface(__uuidof(IDebugRegisters),
                                      reinterpret_cast<void**>(&debug_registers_));
    if (FAILED(hr)) {
        LOG_ERROR("Extension", "Failed to get IDebugRegisters interface");
        return ExtensionError::DebuggerInterfaceError;
    }
    
    // Get IDebugSymbols interface
    hr = debug_client_->QueryInterface(__uuidof(IDebugSymbols),
                                      reinterpret_cast<void**>(&debug_symbols_));
    if (FAILED(hr)) {
        LOG_ERROR("Extension", "Failed to get IDebugSymbols interface");
        return ExtensionError::DebuggerInterfaceError;
    }
    
    return ExtensionError::None;
}

/**
 * @brief Initializes core extension components.
 * 
 * This method creates and initializes the core components of the extension:
 * - SessionManager for managing debugging sessions
 * - CommandExecutor for executing WinDbg commands
 * 
 * @return ExtensionError::None on success, ExtensionError::InitializationFailed on failure
 */
ExtensionError ExtensionImpl::initialize_core_components() {
    try {
        LOG_INFO("Extension", "Creating session manager...");
        // Create session manager
        session_manager_ = std::make_shared<SessionManager>();
        
        LOG_INFO("Extension", "Deferring session manager initialization...");
        // SessionManager initialization is deferred to avoid circular dependency
        // with debug interfaces. It will be initialized on first use.
        LOG_INFO("Extension", "Session manager creation completed");
        
        LOG_INFO("Extension", "Creating command executor...");
        // Create command executor
        command_executor_ = std::make_shared<CommandExecutor>(session_manager_);
        LOG_INFO("Extension", "Command executor created successfully");
        
        return ExtensionError::None;
    } catch (...) {
        LOG_ERROR("Extension", "Exception during core components initialization");
        return ExtensionError::InitializationFailed;
    }
}

/**
 * @brief Initializes communication infrastructure.
 * 
 * This method sets up the named pipe server for MCP client communication.
 * It creates the pipe server with appropriate configuration and sets up
 * the message handler for processing incoming commands.
 * 
 * @return ExtensionError::None on success, ExtensionError::CommunicationSetupFailed on failure
 */
ExtensionError ExtensionImpl::initialize_communication() {
    try {
        LOG_INFO("Extension", "Creating pipe server config...");
        // Create pipe server
        communication::PipeServerConfig config;
        config.pipe_name = R"(\\.\pipe\vibedbg_debug)";
        config.max_connections = 10;
        
        LOG_INFO("Extension", "Creating pipe server instance...");
        pipe_server_ = std::make_unique<NamedPipeServer>(config);
        
        LOG_INFO("Extension", "Setting message handler...");
        // Set message handler
        pipe_server_->set_message_handler(
            [this](const CommandRequest& request, ErrorCode* error) -> CommandResponse {
                return handle_mcp_command(request, error);
            }
        );
        
        LOG_INFO("Extension", "Starting pipe server...");
        // Start the server (async - don't wait for full initialization)
        PipeServerError pipe_error = pipe_server_->start();
        if (pipe_error != PipeServerError::None) {
            LOG_ERROR_DETAIL("Extension", "Failed to start named pipe server", "Error code: " + std::to_string(static_cast<int>(pipe_error)));
            return ExtensionError::CommunicationSetupFailed;
        }
        
        // Give the server a moment to initialize but don't wait indefinitely
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LOG_INFO("Extension", "Pipe server started successfully");
        return ExtensionError::None;
    } catch (...) {
        LOG_ERROR("Extension", "Exception during communication initialization");
        return ExtensionError::CommunicationSetupFailed;
    }
}

/**
 * @brief Handles MCP command requests from the named pipe server.
 * 
 * This method processes incoming MCP commands from the named pipe server.
 * It validates the extension state, routes commands to appropriate handlers,
 * and returns structured responses. The method also tracks connection
 * statistics for monitoring purposes.
 * 
 * @param[in] request The command request to process
 * @param[out] error Optional pointer to receive error information
 * 
 * @return CommandResponse containing the result of command execution
 */
CommandResponse ExtensionImpl::handle_mcp_command(
    _In_ const CommandRequest& request, 
    _Out_opt_ ErrorCode* error) {
    CommandResponse response;
    response.request_id = request.request_id;
    response.timestamp = std::chrono::steady_clock::now();
    
    // Log command reception via OutputDebugString for DebugView
    LOG_INFO("MCP", "Received MCP command: " + request.command);
    
    try {
        if (!initialized_.load()) {
            response.success = false;
            response.error_message = "Extension not initialized";
            LOG_ERROR("MCP", "Command rejected - extension not initialized");
            if (error) *error = ErrorCode::ExtensionNotLoaded;
            return response;
        }
        
        // Use the new generic command system for LLM-driven debugging
        if (!command_executor_) {
            response.success = false;
            response.error_message = "Command executor not available";
            if (error) *error = ErrorCode::InternalError;
            return response;
        }
        
        // Create command handlers if not already created
        if (!command_handlers_) {
            command_handlers_ = std::make_unique<CommandHandlers>(session_manager_, command_executor_);
        }
        
        // Execute using the LLM-friendly command system
        LOG_INFO("MCP", "Executing command via LLM handler");
        auto output = command_handlers_->handle_llm_command(request.command, true);
        
        if (!output.empty()) {
            response.success = true;
            response.output = output;
            if (error) *error = ErrorCode::None;
            
            LOG_INFO_DETAIL("MCP", "Command executed successfully", "Output length: " + std::to_string(output.length()));
        } else {
            response.success = false;
            response.error_message = "Command execution failed or returned no output";
            LOG_ERROR("MCP", "Command execution failed or returned empty result");
            if (error) *error = ErrorCode::CommandFailed;
        }
        
        // Update stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_connections++;
        }
        
        return response;
    } catch (...) {
        response.success = false;
        response.error_message = "Internal error during command processing";
        if (error) *error = ErrorCode::InternalError;
        return response;
    }
}

/**
 * @brief Cleans up WinDbg debugger interfaces.
 * 
 * This method releases all acquired WinDbg interfaces in the reverse order
 * of acquisition to avoid dependency issues. It properly handles the
 * reference counting by calling Release() on each interface.
 */
void ExtensionImpl::cleanup_interfaces() {
    // Cleanup in reverse order of initialization to avoid dependency issues
    if (debug_symbols_) {
        debug_symbols_->Release();
        debug_symbols_ = nullptr;
    }
    
    if (debug_registers_) {
        debug_registers_->Release();
        debug_registers_ = nullptr;
    }
    
    if (debug_data_spaces_) {
        debug_data_spaces_->Release();
        debug_data_spaces_ = nullptr;
    }
    
    if (debug_control_) {
        debug_control_->Release();
        debug_control_ = nullptr;
    }
    
    // Don't release debug_client_ - we don't own it, it's passed from WinDbg
    debug_client_ = nullptr;
}

/**
 * @brief Cleans up core extension components.
 * 
 * This method cleans up the core components in the reverse order of
 * initialization. It uses smart pointers to ensure proper cleanup
 * and resource deallocation.
 */
void ExtensionImpl::cleanup_components() {
    // Cleanup in reverse order of initialization
    command_handlers_.reset();
    command_executor_.reset();
    session_manager_.reset();
}

/**
 * @brief Cleans up communication infrastructure.
 * 
 * This method stops the named pipe server and cleans up all communication
 * resources. It ensures that all client connections are properly closed
 * and the pipe server is stopped gracefully.
 */
void ExtensionImpl::cleanup_communication() {
    if (pipe_server_) {
        pipe_server_->stop();
        pipe_server_.reset();
    }
}

