#pragma once

#include <string>
#include <future>
#include <chrono>
#include <functional>
#include <vector>
#include <optional>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "session_manager.h"
#include "json.h"

namespace vibedbg::core {

using json = nlohmann::json;

enum class ExecutionError : uint32_t {
    None = 0,
    CommandFailed = 1,
    Timeout = 2,
    InvalidCommand = 3,
    DebuggerNotAttached = 4,
    InternalError = 5,
    Cancelled = 6
};

struct ExecutionOptions {
    std::chrono::milliseconds timeout{30000};
    bool validate_command{true};
    bool capture_detailed_output{false};
    int retry_count{0};
    std::chrono::milliseconds retry_delay{1000};
};

struct CommandResult {
    bool success{false};
    std::string output;
    std::string error_message;
    std::chrono::milliseconds execution_time{0};
    uint32_t exit_code{0};
    std::string command_executed;
    json metadata;
    std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

struct BatchResult {
    std::vector<CommandResult> results;
    size_t successful_commands{0};
    size_t failed_commands{0};
    std::chrono::milliseconds total_execution_time{0};
    bool all_successful{false};
};

class CommandExecutor {
public:
    using CommandCallback = std::function<void(const CommandResult&)>;
    using ProgressCallback = std::function<void(size_t completed, size_t total)>;

    explicit CommandExecutor(std::shared_ptr<SessionManager> session_manager);
    ~CommandExecutor();

    // Synchronous execution
    CommandResult execute_command(std::string_view command, const ExecutionOptions& options = {}, ExecutionError* error = nullptr);

    // Asynchronous execution
    std::future<CommandResult> execute_command_async(std::string_view command, const ExecutionOptions& options = {});

    // Batch execution
    BatchResult execute_batch(const std::vector<std::string>& commands, 
                     const ExecutionOptions& options = {},
                     ProgressCallback progress_callback = nullptr,
                     ExecutionError* error = nullptr);

    std::future<BatchResult> execute_batch_async(const std::vector<std::string>& commands,
                           const ExecutionOptions& options = {},
                           ProgressCallback progress_callback = nullptr);

    // Command validation and preparation
    std::string prepare_command(std::string_view raw_command, ExecutionError* error = nullptr);
    
    bool validate_command_syntax(std::string_view command);
    std::vector<std::string> get_command_suggestions(std::string_view partial_command);

    // Execution control
    void cancel_all_pending();
    size_t get_pending_count() const;
    bool is_busy() const;

    // Performance and monitoring
    struct ExecutorStats {
        uint64_t total_commands_executed = 0;
        uint64_t successful_commands = 0;
        uint64_t failed_commands = 0;
        uint64_t timed_out_commands = 0;
        std::chrono::milliseconds total_execution_time{0};
        std::chrono::milliseconds average_execution_time{0};
        std::chrono::time_point<std::chrono::steady_clock> start_time;
    };

    ExecutorStats get_stats() const;
    void reset_stats();

private:
    std::shared_ptr<SessionManager> session_manager_;
    
    // Async execution management
    std::queue<std::function<void()>> pending_tasks_;
    mutable std::mutex tasks_mutex_;
    std::condition_variable tasks_cv_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ExecutorStats stats_;

    // Core execution implementation
    CommandResult execute_command_internal(std::string_view command, const ExecutionOptions& options, ExecutionError* error = nullptr);

    std::string execute_windbg_command(std::string_view command, std::chrono::milliseconds timeout, ExecutionError* error = nullptr);

    // Command processing pipeline
    std::string validate_and_prepare_command(std::string_view command, const ExecutionOptions& options, ExecutionError* error = nullptr);

    std::string sanitize_command(std::string_view command, ExecutionError* error = nullptr);

    std::chrono::milliseconds 
        get_timeout_for_command(std::string_view command, const ExecutionOptions& options);

    // Retry logic
    CommandResult execute_with_retry(std::string_view command, const ExecutionOptions& options, ExecutionError* error = nullptr);

    // Worker thread management
    void start_worker_threads(size_t thread_count = 2);
    void stop_worker_threads();
    void worker_thread_loop();

    // Statistics helpers
    void update_stats_on_success(const CommandResult& result);
    void update_stats_on_failure(ExecutionError error);

    // Utility methods
    bool is_dangerous_command(std::string_view command) const;
    bool requires_special_handling(std::string_view command) const;
    std::string extract_command_name(std::string_view command) const;
};

// Command validation utilities
namespace command_validation {
    bool is_read_only_command(std::string_view command);
    bool is_state_changing_command(std::string_view command);
    bool is_potentially_harmful_command(std::string_view command);
    std::vector<std::string> get_safe_commands_for_automation();
    std::optional<std::string> get_command_description(std::string_view command);
}

// Timeout calculation utilities
namespace timeout_utils {
    std::chrono::milliseconds get_default_timeout_for_command(std::string_view command);
    std::chrono::milliseconds calculate_adaptive_timeout(std::string_view command);
    bool is_long_running_command(std::string_view command);
}

} // namespace vibedbg::core