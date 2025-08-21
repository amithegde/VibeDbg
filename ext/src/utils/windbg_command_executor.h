#pragma once

#include "pch.h"
#include "output_capture.h"
#include <string>

/**
 * @class WinDbgCommandExecutor
 * @brief Executes WinDbg commands and captures output.
 */
class WinDbgCommandExecutor {
public:
    WinDbgCommandExecutor();
    ~WinDbgCommandExecutor();

    /**
     * @brief Execute a WinDbg command and capture its output.
     * @param command The command to execute
     * @param timeout_ms Timeout in milliseconds (default: 30 seconds)
     * @return Captured output or error message
     */
    std::string ExecuteCommand(const std::string& command, DWORD timeout_ms = 30000);

    /**
     * @brief Execute a command without output capture (for simple commands).
     * @param command The command to execute
     * @return HRESULT indicating success/failure
     */
    HRESULT ExecuteCommandSimple(const std::string& command);

    /**
     * @brief Check if the executor is properly initialized.
     * @return true if initialized, false otherwise
     */
    bool IsInitialized() const;

private:
    HRESULT InitializeInterfaces();
    void CleanupInterfaces();

    IDebugClient* debug_client_;
    IDebugControl* debug_control_;
    bool initialized_;
};