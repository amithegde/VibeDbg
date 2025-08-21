#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <optional>
#include <shared_mutex>
#include <atomic>
#include "json.h"

namespace vibedbg::core {

using json = nlohmann::json;

enum class SessionError : uint32_t {
    None = 0,
    InitializationFailed = 1,
    InvalidState = 2,
    CommandValidationFailed = 3,
    ContextSwitchFailed = 4,
    InternalError = 5
};

struct ProcessInfo {
    uint32_t process_id = 0;
    std::string process_name;
    std::string image_path;
    bool is_attached = false;
    std::chrono::time_point<std::chrono::steady_clock> attach_time;
};

struct ThreadInfo {
    uint32_t thread_id = 0;
    uint32_t process_id = 0;
    bool is_current = false;
    std::string state;
    uintptr_t stack_base = 0;
    uintptr_t stack_limit = 0;
};

struct SessionState {
    std::optional<ProcessInfo> current_process;
    std::optional<ThreadInfo> current_thread;
    std::chrono::time_point<std::chrono::steady_clock> session_start;
    bool is_connected{false};
    bool is_target_running{false};
    json metadata;
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager() = default;

    // Session lifecycle
    SessionError initialize();
    void shutdown();
    bool is_initialized() const noexcept { return initialized_.load(); }

    // State management  
    const SessionState& get_state() const;
    SessionError update_state(const SessionState& new_state);
    
    // Command suggestions
    std::vector<std::string> get_suggested_commands() const;

    // Process and thread context management
    SessionError switch_to_process(uint32_t process_id);
    SessionError switch_to_thread(uint32_t thread_id);
    ProcessInfo get_current_process_info(SessionError* error = nullptr);
    ThreadInfo get_current_thread_info(SessionError* error = nullptr);

    // Session recovery and persistence
    SessionError save_session_state();
    SessionError restore_session_state();
    json serialize_state() const;
    SessionError deserialize_state(const json& state_data);

    // Event notifications
    using StateChangeCallback = std::function<void(const SessionState&, const SessionState&)>;
    void register_state_change_callback(StateChangeCallback callback);

private:
    mutable std::shared_mutex state_mutex_;
    SessionState state_;
    std::atomic<bool> initialized_{false};
    std::vector<StateChangeCallback> state_change_callbacks_;

    // WinDbg interface helpers
    std::string execute_windbg_command(std::string_view command, SessionError* error = nullptr);
    bool is_debugger_attached_to_target();
    
    // Internal initialization
    void ensure_initialized();
    
    // Command validation helpers
    bool is_dangerous_command(std::string_view command) const;
    
    // State change notification
    void notify_state_change(const SessionState& old_state, const SessionState& new_state);
};

// No utility functions needed for user mode only debugging

} // namespace vibedbg::core