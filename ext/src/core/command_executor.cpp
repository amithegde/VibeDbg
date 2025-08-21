#include "pch.h"
#include "../../inc/command_executor.h"
#include "../../inc/error_handling.h"
#include "../utils/windbg_helpers.h"

using namespace vibedbg::core;
using namespace vibedbg::utils;

CommandExecutor::CommandExecutor(std::shared_ptr<SessionManager> session_manager)
    : session_manager_(std::move(session_manager))
    , shutdown_requested_(false) {
    
    stats_.start_time = std::chrono::steady_clock::now();
    start_worker_threads(2); // Start 2 worker threads
}

CommandExecutor::~CommandExecutor() {
    stop_worker_threads();
}

CommandResult CommandExecutor::execute_command(std::string_view command, const ExecutionOptions& options, ExecutionError* error) {
    ExecutionError exec_error = ExecutionError::None;
    CommandResult result = execute_command_internal(command, options, &exec_error);
    if (error) *error = exec_error;
    return result;
}

std::future<CommandResult> CommandExecutor::execute_command_async(std::string_view command, const ExecutionOptions& options) {
    auto promise = std::make_shared<std::promise<CommandResult>>();
    auto future = promise->get_future();
    
    // Queue task for worker thread
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        pending_tasks_.push([this, command = std::string(command), options, promise]() {
            ExecutionError error;
            auto result = execute_command_internal(command, options, &error);
            promise->set_value(result);
        });
    }
    tasks_cv_.notify_one();
    
    return future;
}

BatchResult CommandExecutor::execute_batch(const std::vector<std::string>& commands, 
                              const ExecutionOptions& options,
                              ProgressCallback progress_callback,
                              ExecutionError* error) {
    BatchResult batch_result;
    batch_result.results.reserve(commands.size());
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < commands.size(); ++i) {
        ExecutionError exec_error;
        auto result = execute_command_internal(commands[i], options, &exec_error);
        
        batch_result.results.push_back(result);
        if (result.success) {
            batch_result.successful_commands++;
        } else {
            batch_result.failed_commands++;
        }
        
        // Update progress
        if (progress_callback) {
            progress_callback(i + 1, commands.size());
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    batch_result.total_execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    batch_result.all_successful = (batch_result.failed_commands == 0);
    
    if (error) *error = ExecutionError::None;
    return batch_result;
}

std::future<BatchResult> CommandExecutor::execute_batch_async(const std::vector<std::string>& commands,
                                    const ExecutionOptions& options,
                                    ProgressCallback progress_callback) {
    auto promise = std::make_shared<std::promise<BatchResult>>();
    auto future = promise->get_future();
    
    // Queue batch task
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        pending_tasks_.push([this, commands, options, progress_callback, promise]() {
            ExecutionError error;
            auto result = execute_batch(commands, options, progress_callback, &error);
            promise->set_value(result);
        });
    }
    tasks_cv_.notify_one();
    
    return future;
}

std::string CommandExecutor::prepare_command(std::string_view raw_command, ExecutionError* error) {
    if (!session_manager_) {
        if (error) *error = ExecutionError::InternalError;
        return "";
    }
    
    // Sanitize the command
    ExecutionError sanitize_error;
    auto sanitized = sanitize_command(raw_command, &sanitize_error);
    if (sanitize_error != ExecutionError::None) {
        if (error) *error = sanitize_error;
        return "";
    }
    
    if (error) *error = ExecutionError::None;
    return sanitized;
}

bool CommandExecutor::validate_command_syntax(std::string_view command) {
    // Basic syntax validation
    if (command.empty()) {
        return false;
    }
    
    // Check for dangerous commands
    if (is_dangerous_command(command)) {
        return false;
    }
    
    // Check command length (use simple constant)
    if (command.length() > 1024) {
        return false;
    }
    
    return true;
}

std::vector<std::string> CommandExecutor::get_command_suggestions(std::string_view partial_command) {
    if (!session_manager_) {
        return {};
    }
    
    auto suggested = session_manager_->get_suggested_commands();
    
    // Filter suggestions based on partial command
    std::vector<std::string> filtered;
    for (const auto& suggestion : suggested) {
        if (suggestion.find(partial_command) == 0) {
            filtered.push_back(suggestion);
        }
    }
    
    return filtered;
}

void CommandExecutor::cancel_all_pending() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    // Clear the queue
    while (!pending_tasks_.empty()) {
        pending_tasks_.pop();
    }
}

size_t CommandExecutor::get_pending_count() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    return pending_tasks_.size();
}

bool CommandExecutor::is_busy() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    return !pending_tasks_.empty();
}

CommandExecutor::ExecutorStats CommandExecutor::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto stats = stats_;
    
    // Calculate average execution time
    if (stats.total_commands_executed > 0) {
        stats.average_execution_time = std::chrono::milliseconds(
            stats.total_execution_time.count() / stats.total_commands_executed);
    }
    
    return stats;
}

void CommandExecutor::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ExecutorStats{};
    stats_.start_time = std::chrono::steady_clock::now();
}

// Private implementation methods

CommandResult CommandExecutor::execute_command_internal(std::string_view command, const ExecutionOptions& options, ExecutionError* error) {
            LOG_DEBUG("CommandExecutor", "Starting internal execution of: " + std::string(command));
    
    CommandResult result;
    result.command_executed = std::string(command);
    result.timestamp = std::chrono::steady_clock::now();
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (!session_manager_) {
        LOG_ERROR("CommandExecutor", "No session manager available");
        result.success = false;
        result.error_message = "Session manager not available";
        if (error) *error = ExecutionError::InternalError;
        return result;
    }
    
            LOG_DEBUG("CommandExecutor", "Session manager available");
    
    // Validate command
            LOG_DEBUG("CommandExecutor", "Checking command validation");
    if (options.validate_command && !validate_command_syntax(command)) {
        LOG_ERROR("CommandExecutor", "Command validation failed");
        result.success = false;
        result.error_message = "Invalid command syntax";
        if (error) *error = ExecutionError::InvalidCommand;
        return result;
    }
    
            LOG_DEBUG("CommandExecutor", "Getting session state (this might trigger lazy init)");
    // Prepare command
    auto session_state = session_manager_->get_state();
            LOG_DEBUG("CommandExecutor", "Got session state successfully");
    std::string prepared_command;
    
    // Sanitize and prepare command
    ExecutionError prep_error;
    prepared_command = prepare_command(command, &prep_error);
    if (prep_error != ExecutionError::None) {
        result.success = false;
        result.error_message = "Failed to prepare command";
        if (error) *error = prep_error;
        return result;
    }
    
    // Execute command
    ExecutionError exec_error;
    auto output = execute_windbg_command(prepared_command, options.timeout, &exec_error);
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (exec_error == ExecutionError::None) {
        result.success = true;
        result.output = output;
        update_stats_on_success(result);
    } else {
        result.success = false;
        result.error_message = "Command execution failed";
        update_stats_on_failure(exec_error);
    }
    
    if (error) *error = exec_error;
    return result;
}

std::string CommandExecutor::execute_windbg_command(std::string_view command, std::chrono::milliseconds timeout, ExecutionError* error) {
    try {
        HRESULT hr;
        auto result = WinDbgHelpers::execute_command_with_timeout(command, timeout, &hr);
        if (SUCCEEDED(hr)) {
            if (error) *error = ExecutionError::None;
            return result;
        } else {
            if (error) *error = ExecutionError::CommandFailed;
            return "";
        }
    } catch (...) {
        if (error) *error = ExecutionError::InternalError;
        return "";
    }
}

std::string CommandExecutor::validate_and_prepare_command(std::string_view command, [[maybe_unused]] const ExecutionOptions& options, ExecutionError* error) {
    if (!validate_command_syntax(command)) {
        if (error) *error = ExecutionError::InvalidCommand;
        return "";
    }
    
    ExecutionError sanitize_error;
    auto sanitized = sanitize_command(command, &sanitize_error);
    if (sanitize_error != ExecutionError::None) {
        if (error) *error = sanitize_error;
        return "";
    }
    
    if (error) *error = ExecutionError::None;
    return sanitized;
}

std::string CommandExecutor::sanitize_command(std::string_view command, ExecutionError* error) {
    // Basic sanitization
    std::string sanitized(command);
    
    // Remove potentially dangerous characters or patterns
    // This is a simplified implementation
    if (sanitized.find("rm ") != std::string::npos || 
        sanitized.find("del ") != std::string::npos) {
        if (error) *error = ExecutionError::InvalidCommand;
        return "";
    }
    
    if (error) *error = ExecutionError::None;
    return sanitized;
}

CommandResult CommandExecutor::execute_with_retry(std::string_view command, const ExecutionOptions& options, ExecutionError* error) {
    ExecutionError last_error = ExecutionError::None;
    
    for (int attempt = 0; attempt <= options.retry_count; ++attempt) {
        auto result = execute_command_internal(command, options, &last_error);
        
        if (last_error == ExecutionError::None || last_error != ExecutionError::Timeout) {
            if (error) *error = last_error;
            return result;
        }
        
        // Wait before retry
        if (attempt < options.retry_count) {
            std::this_thread::sleep_for(options.retry_delay);
        }
    }
    
    // All retries failed
    CommandResult failed_result;
    failed_result.success = false;
    failed_result.error_message = "Command failed after retries";
    failed_result.command_executed = std::string(command);
    
    if (error) *error = last_error;
    return failed_result;
}

void CommandExecutor::start_worker_threads(size_t thread_count) {
    shutdown_requested_ = false;
    worker_threads_.reserve(thread_count);
    
    for (size_t i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back(&CommandExecutor::worker_thread_loop, this);
    }
}

void CommandExecutor::stop_worker_threads() {
    shutdown_requested_ = true;
    tasks_cv_.notify_all();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

void CommandExecutor::worker_thread_loop() {
    while (!shutdown_requested_) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(tasks_mutex_);
            tasks_cv_.wait(lock, [this] { return !pending_tasks_.empty() || shutdown_requested_; });
            
            if (shutdown_requested_ && pending_tasks_.empty()) {
                break;
            }
            
            if (!pending_tasks_.empty()) {
                task = std::move(pending_tasks_.front());
                pending_tasks_.pop();
            }
        }
        
        if (task) {
            task();
        }
    }
}

void CommandExecutor::update_stats_on_success(const CommandResult& result) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_commands_executed++;
    stats_.successful_commands++;
    stats_.total_execution_time += result.execution_time;
}

void CommandExecutor::update_stats_on_failure(ExecutionError error) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_commands_executed++;
    stats_.failed_commands++;
    
    if (error == ExecutionError::Timeout) {
        stats_.timed_out_commands++;
    }
}

bool CommandExecutor::is_dangerous_command(std::string_view command) const {
    // Check for dangerous commands that could modify memory or system state
    static const std::vector<std::string_view> dangerous_prefixes = {
        "ed ", "eb ", "ew ", "eq ",  // Memory editing commands
        ".reboot", ".crash",         // System control
        "!process 0 7",             // Can cause system freeze
        ".detach", ".kill",         // Process control
        "sxe", "sxd",              // Exception handling changes
    };
    
    // Convert command to lowercase for comparison
    std::string lower_cmd;
    lower_cmd.reserve(command.size());
    std::transform(command.begin(), command.end(), std::back_inserter(lower_cmd), 
                   [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    
    for (const auto& prefix : dangerous_prefixes) {
        if (lower_cmd.find(prefix) == 0) {
            return true;
        }
    }
    
    return false;
}

bool CommandExecutor::requires_special_handling(std::string_view command) const {
    return command.find("g") == 0 || 
           command.find("p") == 0 ||
           command.find("t") == 0;
}

std::string CommandExecutor::extract_command_name(std::string_view command) const {
    auto space_pos = command.find(' ');
    if (space_pos != std::string_view::npos) {
        return std::string(command.substr(0, space_pos));
    }
    return std::string(command);
}

// Command validation utilities implementation
namespace vibedbg::core::command_validation {
    bool is_read_only_command(std::string_view command) {
        return command.find("r") == 0 || 
               command.find("u") == 0 ||
               command.find("d") == 0 ||
               command.find("k") == 0;
    }
    
    bool is_state_changing_command(std::string_view command) {
        return command.find("g") == 0 || 
               command.find("p") == 0 ||
               command.find("t") == 0;
    }
    
    bool is_potentially_harmful_command(std::string_view command) {
        return command.find("!") == 0 || 
               command.find("ed ") == 0;
    }
    
    std::vector<std::string> get_safe_commands_for_automation() {
        return {"r", "u", "d", "k", "lm", "dt", "!peb"};
    }
    
    std::optional<std::string> get_command_description(std::string_view command) {
        if (command == "r") return "Display registers";
        if (command == "u") return "Unassemble";
        if (command == "d") return "Display memory";
        return std::nullopt;
    }
}

// Timeout utilities implementation
namespace vibedbg::core::timeout_utils {
    std::chrono::milliseconds get_default_timeout_for_command(std::string_view command) {
        if (is_long_running_command(command)) {
            return std::chrono::milliseconds(60000); // 60 seconds
        }
        return std::chrono::milliseconds(5000); // 5 seconds
    }
    
    std::chrono::milliseconds calculate_adaptive_timeout(std::string_view command) {
        return get_default_timeout_for_command(command);
    }
    
    bool is_long_running_command(std::string_view command) {
        return command.find("g") == 0 || 
               command.find("!analyze") == 0;
    }
}