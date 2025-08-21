#include "pch.h"
#include "windbg_command_executor.h"
#include "command_utils.h"
#include "constants.h"
#include "output_capture.h"

using namespace vibedbg::utils;

/**
 * @brief Constructs a WinDbg command executor and initializes debugger interfaces.
 * 
 * This constructor creates the WinDbg command executor and immediately
 * attempts to initialize the required debugger interfaces. The initialization
 * is performed in the constructor to ensure the executor is ready for use
 * as soon as it's created.
 */
WinDbgCommandExecutor::WinDbgCommandExecutor() 
    : debug_client_(nullptr), debug_control_(nullptr), initialized_(false) {
    InitializeInterfaces();
}

/**
 * @brief Destructor that ensures proper cleanup of debugger interfaces.
 * 
 * This destructor calls CleanupInterfaces() to ensure all acquired
 * debugger interfaces are properly released and resources are cleaned up.
 */
WinDbgCommandExecutor::~WinDbgCommandExecutor() {
    CleanupInterfaces();
}

/**
 * @brief Executes a WinDbg command with comprehensive error handling and logging.
 * 
 * This method provides a safe interface for executing WinDbg commands through
 * the debugger control interface. It includes command validation, output
 * capture, error handling, and detailed logging for debugging purposes.
 * 
 * The method performs the following steps:
 * 1. Validates the executor is initialized
 * 2. Validates the command is safe to execute
 * 3. Creates an output capture helper
 * 4. Executes the command through the debug control interface
 * 5. Captures and formats the output
 * 6. Logs the results for debugging
 * 
 * @param[in] command The WinDbg command to execute
 * @param[in] timeout_ms Maximum time to wait for command completion (unused)
 * 
 * @return Formatted result of command execution, or error message on failure
 * 
 * @note This method is thread-safe and handles exceptions gracefully.
 */
std::string WinDbgCommandExecutor::ExecuteCommand(
    _In_ const std::string& command, 
    _In_ [[maybe_unused]] DWORD timeout_ms) {
    CommandUtils::log_command_start(command);
    
    if (!initialized_) {
        return CommandUtils::format_error_message("WinDbg interfaces not initialized");
    }

    if (!CommandUtils::is_command_safe(command)) {
        return CommandUtils::format_error_message("Invalid or unsafe command");
    }

    try {
        LOG_DEBUG("WinDbgCommandExecutor", "Creating output capture helper");
        // Create output capture helper
        OutputCaptureHelper capture_helper(debug_client_);
        
        LOG_DEBUG("WinDbgCommandExecutor", "Calling debug_control_->Execute()");
        // Execute the command
        HRESULT hr = debug_control_->Execute(
            DEBUG_OUTCTL_THIS_CLIENT,
            command.c_str(),
            DEBUG_EXECUTE_DEFAULT
        );

        if (FAILED(hr)) {
            CommandUtils::log_command_result(command, false, 0);
            return CommandUtils::format_error_message("Command execution failed (HRESULT: 0x" + 
                   std::to_string(hr) + ")");
        }

        LOG_DEBUG("WinDbgCommandExecutor", "Getting captured output");
        // Get captured output
        std::string output = capture_helper.GetCapturedOutput();
        
        CommandUtils::log_command_result(command, true, output.length());
        
        return CommandUtils::format_success_message(command, output);
    }
    catch (const std::exception& e) {
        CommandUtils::log_command_result(command, false, 0);
        return CommandUtils::format_error_message("Exception during command execution: " + std::string(e.what()));
    }
    catch (...) {
        CommandUtils::log_command_result(command, false, 0);
        return CommandUtils::format_error_message("Unknown exception during command execution");
    }
}

/**
 * @brief Executes a WinDbg command with simple error handling.
 * 
 * This method provides a lightweight interface for executing WinDbg commands
 * when detailed output formatting is not required. It returns the raw HRESULT
 * from the debug control interface for direct error handling by the caller.
 * 
 * @param[in] command The WinDbg command to execute
 * 
 * @return HRESULT from the debug control interface execution
 * 
 * @note This method requires the executor to be initialized and the command
 *       to be non-empty. It does not perform command safety validation.
 */
HRESULT WinDbgCommandExecutor::ExecuteCommandSimple(_In_ const std::string& command) {
    if (!initialized_) {
        return E_FAIL;
    }

    if (command.empty()) {
        return E_INVALIDARG;
    }

    return debug_control_->Execute(
        DEBUG_OUTCTL_THIS_CLIENT,
        command.c_str(),
        DEBUG_EXECUTE_DEFAULT
    );
}

/**
 * @brief Checks if the command executor is properly initialized.
 * 
 * @return true if initialized and ready for use, false otherwise
 */
bool WinDbgCommandExecutor::IsInitialized() const {
    return initialized_;
}

/**
 * @brief Initializes the WinDbg debugger interfaces.
 * 
 * This method creates and acquires the necessary WinDbg debugger interfaces
 * required for command execution. It follows a specific initialization order:
 * 1. Creates the debug client interface
 * 2. Queries for the debug control interface
 * 
 * If any step fails, the method performs cleanup and returns the appropriate
 * HRESULT error code.
 * 
 * @return HRESULT S_OK on success, appropriate error code on failure
 * 
 * @note This method is called automatically during construction and should
 *       not be called manually unless re-initialization is required.
 */
HRESULT WinDbgCommandExecutor::InitializeInterfaces() {
    // Create debug client
    HRESULT hr = DebugCreate(__uuidof(IDebugClient), (void**)&debug_client_);
    if (FAILED(hr)) {
        return hr;
    }

    // Get control interface
    hr = debug_client_->QueryInterface(__uuidof(IDebugControl), (void**)&debug_control_);
    if (FAILED(hr)) {
        CleanupInterfaces();
        return hr;
    }

    initialized_ = true;
    return S_OK;
}

/**
 * @brief Cleans up the WinDbg debugger interfaces.
 * 
 * This method releases all acquired debugger interfaces in the reverse order
 * of acquisition to avoid dependency issues. It properly handles the
 * reference counting by calling Release() on each interface and resets
 * the initialization state.
 * 
 * @note This method is called automatically during destruction and can be
 *       called manually for cleanup if needed.
 */
void WinDbgCommandExecutor::CleanupInterfaces() {
    if (debug_control_) {
        debug_control_->Release();
        debug_control_ = nullptr;
    }

    if (debug_client_) {
        debug_client_->Release();
        debug_client_ = nullptr;
    }

    initialized_ = false;
}