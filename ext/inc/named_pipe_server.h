#pragma once

#include <Windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <span>
#include <chrono>
#include "message_protocol.h"
#include "handle_wrapper.h"

namespace vibedbg::communication {

class ClientConnection;

enum class PipeServerError : uint32_t {
    None = 0,
    CreationFailed = 1,
    ConnectionFailed = 2,
    ReadFailed = 3,
    WriteFailed = 4,
    Timeout = 5,
    Disconnected = 6
};

struct PipeServerConfig {
    std::string pipe_name = R"(\\.\pipe\vibedbg_debug)";
    uint32_t max_connections = 10;
    uint32_t buffer_size = 64 * 1024; // 64KB
    std::chrono::milliseconds read_timeout{30000};
    std::chrono::milliseconds write_timeout{5000};
    bool enable_heartbeat = true;
    std::chrono::milliseconds heartbeat_interval{10000};
};

class NamedPipeServer {
public:
    using MessageHandler = std::function<CommandResponse(const CommandRequest&, ErrorCode* error)>;

    explicit NamedPipeServer(const PipeServerConfig& config = {});
    ~NamedPipeServer();

    // Server lifecycle
    PipeServerError start();
    void stop();
    bool is_running() const noexcept { return running_.load(); }

    // Message handling
    void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }

    // Statistics and monitoring
    struct ServerStats {
        uint64_t total_connections = 0;
        uint64_t active_connections = 0;
        uint64_t total_messages_processed = 0;
        uint64_t total_errors = 0;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
        std::chrono::milliseconds uptime{0};
    };

    ServerStats get_stats() const;
    std::vector<std::string> get_active_connection_ids() const;

    // Configuration
    const PipeServerConfig& get_config() const noexcept { return config_; }

private:
    PipeServerConfig config_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> server_thread_;
    MessageHandler message_handler_;

    // Connection management
    mutable std::shared_mutex connections_mutex_;
    std::vector<std::unique_ptr<ClientConnection>> connections_;
    std::vector<std::thread> client_threads_;

    // Statistics
    mutable std::mutex stats_mutex_;
    ServerStats stats_;

    // Server implementation
    void server_loop();
    HANDLE create_pipe_instance(PipeServerError* error = nullptr);
    void handle_client_connection(HANDLE pipe_handle);
    void cleanup_disconnected_connections();

    // Message processing
    PipeServerError process_client_messages(ClientConnection& client);
    CommandResponse handle_command(const CommandRequest& request, ErrorCode* error = nullptr);
    void send_heartbeat(ClientConnection& client);

    // Error handling
    void update_stats_on_error();
    void update_stats_on_message();
    void update_stats_on_connection();
};

class ClientConnection {
public:
    explicit ClientConnection(HANDLE pipe_handle, const std::string& connection_id);
    ~ClientConnection();

    // Connection management
    bool is_active() const noexcept { return active_.load(); }
    void mark_inactive() { active_.store(false); }
    const std::string& get_id() const noexcept { return connection_id_; }
    HANDLE get_handle() const noexcept { return pipe_handle_.get(); }

    // Message I/O
    std::vector<std::byte> read_message(std::chrono::milliseconds timeout, PipeServerError* error = nullptr);
    
    PipeServerError write_message(std::span<const std::byte> data, std::chrono::milliseconds timeout);

    // Statistics
    struct ConnectionStats {
        std::chrono::time_point<std::chrono::steady_clock> connection_time;
        uint64_t messages_received = 0;
        uint64_t messages_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t bytes_sent = 0;
        std::chrono::time_point<std::chrono::steady_clock> last_activity;
    };

    const ConnectionStats& get_stats() const noexcept { return stats_; }

private:
    utils::HandleWrapper pipe_handle_;
    std::string connection_id_;
    std::atomic<bool> active_{true};
    
    // I/O buffers
    std::vector<std::byte> read_buffer_;
    std::vector<std::byte> message_buffer_;
    
    // Statistics
    ConnectionStats stats_;
    mutable std::mutex stats_mutex_;

    // I/O implementation
    void update_read_stats(size_t bytes_read);
    void update_write_stats(size_t bytes_written);
    bool find_complete_message();
};

// Utility functions
std::string generate_connection_id();
std::string format_pipe_error(DWORD error_code);
bool is_pipe_error_recoverable(DWORD error_code);

} // namespace vibedbg::communication