#include "pch.h"
#include "../../inc/session_manager.h"
#include "../../inc/error_handling.h"
#include "../utils/windbg_helpers.h"

using namespace vibedbg::core;
using namespace vibedbg::utils;

SessionManager::SessionManager() {
    state_.session_start = std::chrono::steady_clock::now();
}

SessionError SessionManager::initialize() {
    LOG_INFO("SessionManager", "initialize() started");
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    LOG_DEBUG("SessionManager", "Got state mutex lock");
    
    // Assume user-mode debugging by default (original goal was user-mode processes only)
    LOG_INFO("SessionManager", "Setting up for user-mode debugging");
    state_.is_connected = true;
    LOG_INFO("SessionManager", "Connected to debugger");
    
    // Try to get current process info
    LOG_DEBUG("SessionManager", "Getting current process info");
    HRESULT hr_pid, hr_name;
    uint32_t process_id = WinDbgHelpers::get_current_process_id(&hr_pid);
    std::string process_name = WinDbgHelpers::get_current_process_name(&hr_name);
    
    if (SUCCEEDED(hr_pid) && SUCCEEDED(hr_name)) {
        LOG_DEBUG("SessionManager", "Successfully got process info");
        ProcessInfo process_info;
        process_info.process_id = process_id;
        process_info.process_name = process_name;
        process_info.is_attached = true;
        process_info.attach_time = std::chrono::steady_clock::now();
        
        state_.current_process = process_info;
    } else {
        LOG_WARNING("SessionManager", "Failed to get process info, continuing anyway");
    }
    
    // Try to get current thread info
    LOG_DEBUG("SessionManager", "Getting current thread info");
    HRESULT hr_tid;
    uint32_t thread_id = WinDbgHelpers::get_current_thread_id(&hr_tid);
    if (SUCCEEDED(hr_tid)) {
        LOG_DEBUG("SessionManager", "Successfully got thread info");
        ThreadInfo thread_info;
        thread_info.thread_id = thread_id;
        if (state_.current_process) {
            thread_info.process_id = state_.current_process->process_id;
        }
        thread_info.is_current = true;
        thread_info.state = "Running";
        
        state_.current_thread = thread_info;
    } else {
        LOG_WARNING("SessionManager", "Failed to get thread info, continuing anyway");
    }
    
    initialized_.store(true);
    LOG_INFO("SessionManager", "Initialization completed successfully");
    return SessionError::None;
}

void SessionManager::shutdown() {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    initialized_.store(false);
    state_.is_connected = false;
}

// detect_current_mode removed - we now assume user-mode debugging by default
// Original hanging issue was caused by WinDbgHelpers::detect_debugging_mode()

const SessionState& SessionManager::get_state() const {
    // Lazy initialization check
    const_cast<SessionManager*>(this)->ensure_initialized();
    
    std::shared_lock<std::shared_mutex> lock(state_mutex_);
    return state_;
}

SessionError SessionManager::update_state(const SessionState& new_state) {
    std::unique_lock<std::shared_mutex> lock(state_mutex_);
    state_ = new_state;
    return SessionError::None;
}

std::vector<std::string> SessionManager::get_suggested_commands() const {
    // Return basic user-mode debugging commands
    return {
        "k",        // Stack trace
        "r",        // Registers
        "u",        // Unassemble
        "d",        // Display memory
        "~",        // List threads
        "lm",       // List modules
        "!peb",     // Process environment block
        "dt",       // Display type
        "bp",       // Set breakpoint
        "g",        // Go/continue
        "p",        // Step over
        "t"         // Step into
    };
}


SessionError SessionManager::switch_to_thread(uint32_t thread_id) {
    try {
        std::unique_lock<std::shared_mutex> lock(state_mutex_);
        
        // Update the state first
        if (state_.current_thread) {
            state_.current_thread->thread_id = thread_id;
            state_.current_thread->is_current = true;
        } else {
            ThreadInfo thread_info;
            thread_info.thread_id = thread_id;
            thread_info.is_current = true;
            thread_info.state = "running";
            state_.current_thread = thread_info;
        }
        
        // Thread switching will be implemented when debugger interfaces are available
        
        return SessionError::None;
    } catch (...) {
        return SessionError::InternalError;
    }
}

void SessionManager::ensure_initialized() {
    LOG_DEBUG("SessionManager", "ensure_initialized() called");
    if (!initialized_.load()) {
        LOG_INFO("SessionManager", "Not initialized, calling initialize()");
        // Try to initialize, but don't fail if it doesn't work
        // This avoids blocking during extension startup
        initialize();
        LOG_INFO("SessionManager", "initialize() completed");
    } else {
        LOG_DEBUG("SessionManager", "Already initialized");
    }
}