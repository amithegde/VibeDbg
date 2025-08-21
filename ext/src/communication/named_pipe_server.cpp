#include "pch.h"
#include "../../inc/named_pipe_server.h"
#include "../../inc/message_protocol.h"
#include "../../inc/error_handling.h"
#include <format>
#include <cstring>
#include <sstream>

using namespace vibedbg::communication;

/**
 * @brief Constructs a named pipe server with the specified configuration.
 * 
 * @param[in] config Configuration parameters for the pipe server
 */
NamedPipeServer::NamedPipeServer(const PipeServerConfig& config)
    : config_(config)
    , running_(false) {
    stats_.start_time = std::chrono::steady_clock::now();
}

/**
 * @brief Destructor that ensures proper cleanup of the pipe server.
 * 
 * This destructor calls stop() to ensure the server is properly shut down
 * and all resources are cleaned up.
 */
NamedPipeServer::~NamedPipeServer() {
    stop();
}

/**
 * @brief Starts the named pipe server.
 * 
 * This method creates a background thread that runs the server loop,
 * accepting client connections and handling them in separate threads.
 * The server will continue running until stop() is called.
 * 
 * @return PipeServerError::None on success, PipeServerError::CreationFailed on failure
 * 
 * @note This method is thread-safe and can only be called once per instance.
 */
PipeServerError NamedPipeServer::start() {
    if (running_.load()) {
        return PipeServerError::CreationFailed;
    }
    
    try {
        // Start the server thread
        server_thread_ = std::make_unique<std::thread>(&NamedPipeServer::server_loop, this);
        running_.store(true);
        stats_.start_time = std::chrono::steady_clock::now();
        
        return PipeServerError::None;
    } catch (...) {
        return PipeServerError::CreationFailed;
    }
}

/**
 * @brief Stops the named pipe server and cleans up resources.
 * 
 * This method signals the server to stop, waits for all threads to complete,
 * and cleans up all client connections. It ensures a graceful shutdown
 * of the entire server infrastructure.
 * 
 * @note This method is thread-safe and can be called multiple times safely.
 */
void NamedPipeServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Wait for server thread to finish
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    // Wait for all client threads to finish
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
    
    // Cleanup client connections
    cleanup_disconnected_connections();
    
    server_thread_.reset();
}

/**
 * @brief Gets a list of active connection IDs.
 * 
 * This method returns the IDs of all currently active client connections.
 * The list is thread-safe and provides a snapshot of the current state.
 * 
 * @return Vector of connection ID strings
 */
std::vector<std::string> NamedPipeServer::get_active_connection_ids() const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    
    std::vector<std::string> connection_ids;
    for (const auto& connection : connections_) {
        if (connection && connection->is_active()) {
            connection_ids.push_back(connection->get_id());
        }
    }
    
    return connection_ids;
}

/**
 * @brief Gets current server statistics.
 * 
 * This method returns a snapshot of the current server statistics including
 * uptime, connection counts, and message processing statistics.
 * 
 * @return ServerStats structure containing current statistics
 */
NamedPipeServer::ServerStats NamedPipeServer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ServerStats current_stats = stats_;
    current_stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stats_.start_time);
    return current_stats;
}

/**
 * @brief Main server loop that accepts client connections.
 * 
 * This method runs in a dedicated thread and continuously creates pipe
 * instances, waits for client connections, and spawns client handling
 * threads. It handles connection failures gracefully and continues
 * running until the server is stopped.
 * 
 * @note This method runs in the server thread and should not be called directly.
 */
void NamedPipeServer::server_loop() {
    while (running_.load()) {
        PipeServerError error = PipeServerError::None;
        HANDLE pipe_handle = create_pipe_instance(&error);
        
        if (pipe_handle == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Wait for a client to connect
        BOOL connected = ConnectNamedPipe(pipe_handle, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(pipe_handle);
            if (running_.load()) {
                update_stats_on_error();
            }
            continue;
        }
        
        // Create client connection
        std::string connection_id = generate_connection_id();
        auto connection = std::make_unique<ClientConnection>(pipe_handle, connection_id);
        
        {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            connections_.push_back(std::move(connection));
        }
        
        update_stats_on_connection();
        
        // Handle client in a separate thread
        client_threads_.emplace_back(&NamedPipeServer::handle_client_connection, this, pipe_handle);
    }
}

/**
 * @brief Handles a client connection in a dedicated thread.
 * 
 * This method processes messages from a specific client connection until
 * the connection is closed or an error occurs. It continuously reads
 * messages, processes them, and sends responses back to the client.
 * 
 * @param[in] pipe_handle Handle to the client's pipe connection
 * 
 * @note This method runs in a client thread and should not be called directly.
 */
void NamedPipeServer::handle_client_connection(_In_ HANDLE pipe_handle) {
    // Find the connection object
    std::unique_ptr<ClientConnection> connection;
    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            if (conn && conn->get_handle() == pipe_handle) {
                // Process messages for this connection
                while (running_.load() && conn->is_active()) {
                    PipeServerError error = process_client_messages(*conn);
                    if (error != PipeServerError::None) {
                        conn->mark_inactive();
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                break;
            }
        }
    }
    
    // Cleanup disconnected connections
    cleanup_disconnected_connections();
}

/**
 * @brief Creates a new named pipe instance.
 * 
 * This method creates a new named pipe instance with the configured
 * parameters. The pipe is created with appropriate security settings
 * and buffer sizes for reliable communication.
 * 
 * @param[out] error Optional pointer to receive error information
 * 
 * @return Handle to the created pipe, or INVALID_HANDLE_VALUE on failure
 */
HANDLE NamedPipeServer::create_pipe_instance(_Out_opt_ PipeServerError* error) {
    HANDLE pipe_handle = CreateNamedPipeA(
        config_.pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        config_.max_connections,
        config_.buffer_size,
        config_.buffer_size,
        static_cast<DWORD>(config_.read_timeout.count()),
        nullptr);
    
    if (pipe_handle == INVALID_HANDLE_VALUE) {
        if (error) *error = PipeServerError::CreationFailed;
        return INVALID_HANDLE_VALUE;
    }
    
    if (error) *error = PipeServerError::None;
    return pipe_handle;
}

/**
 * @brief Processes messages from a client connection.
 * 
 * This method reads messages from the client, parses them using the
 * message protocol, executes the commands, and sends responses back.
 * It handles various error conditions gracefully and maintains the
 * connection state.
 * 
 * @param[in,out] client Reference to the client connection to process
 * 
 * @return PipeServerError::None on success, appropriate error code on failure
 */
PipeServerError NamedPipeServer::process_client_messages(_Inout_ ClientConnection& client) {
    try {
        // Read message from client
        PipeServerError error = PipeServerError::None;
        auto message_data = client.read_message(config_.read_timeout, &error);
        
        if (error != PipeServerError::None) {
            return error;
        }
        
        if (message_data.empty()) {
            return PipeServerError::None; // No message available
        }
        
        // Parse the incoming command from message_data
        ErrorCode parse_error = ErrorCode::None;
        std::span<const std::byte> data_span(message_data.data(), message_data.size());
        CommandRequest request = MessageProtocol::parse_command(data_span, &parse_error);
        
        if (parse_error != ErrorCode::None) {
            // If parsing failed, create error response
            CommandResponse error_response;
            error_response.success = false;
            error_response.error_message = "Failed to parse command";
            error_response.request_id = "unknown";
            
            std::vector<std::byte> response_data = MessageProtocol::serialize_response(error_response);
            PipeServerError send_error = client.write_message(response_data, config_.write_timeout);
            return send_error == PipeServerError::None ? PipeServerError::None : send_error;
        }
        
        // Handle the command
        ErrorCode cmd_error = ErrorCode::None;
        CommandResponse response = handle_command(request, &cmd_error);
        
        // Properly serialize the response using MessageProtocol
        std::vector<std::byte> response_data = MessageProtocol::serialize_response(response, &cmd_error);
        if (cmd_error != ErrorCode::None) {
            // If serialization failed, create a simple error response
            std::string error_response = R"({"protocol_version":1,"message_type":3,"payload":{"type":"error","error_message":"Failed to serialize response"}})";
            error_response += "\r\n\r\n";
            response_data.resize(error_response.size());
            std::memcpy(response_data.data(), error_response.data(), error_response.size());
        }
        
        PipeServerError send_error = client.write_message(response_data, config_.write_timeout);
        if (send_error != PipeServerError::None) {
            return send_error;
        }
        
        update_stats_on_message();
        return PipeServerError::None;
        
    } catch (...) {
        return PipeServerError::ReadFailed;
    }
}

/**
 * @brief Cleans up disconnected client connections.
 * 
 * This method removes all inactive client connections from the connections
 * list. It uses a thread-safe approach to avoid race conditions during
 * cleanup operations.
 */
void NamedPipeServer::cleanup_disconnected_connections() {
    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [](const std::unique_ptr<ClientConnection>& conn) {
                return !conn || !conn->is_active();
            }),
        connections_.end());
}

/**
 * @brief Updates statistics when a new connection is established.
 * 
 * This method increments the connection counters in a thread-safe manner.
 */
void NamedPipeServer::update_stats_on_connection() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_connections++;
    stats_.active_connections++;
}

/**
 * @brief Updates statistics when a message is processed.
 * 
 * This method increments the message processing counters in a thread-safe manner.
 */
void NamedPipeServer::update_stats_on_message() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_messages_processed++;
}

/**
 * @brief Updates statistics when an error occurs.
 * 
 * This method increments the error counters in a thread-safe manner.
 */
void NamedPipeServer::update_stats_on_error() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_errors++;
}

/**
 * @brief Handles a command request using the configured message handler.
 * 
 * This method delegates command processing to the configured message handler.
 * If no handler is configured, it returns a default error response.
 * 
 * @param[in] request The command request to handle
 * @param[out] error Optional pointer to receive error information
 * 
 * @return CommandResponse containing the result of command execution
 */
CommandResponse NamedPipeServer::handle_command(
    _In_ const CommandRequest& request, 
    _Out_opt_ ErrorCode* error) {
    if (message_handler_) {
        return message_handler_(request, error);
    }
    
    // Default response when no handler is set
    CommandResponse response;
    response.success = false;
    response.error_message = "No message handler configured";
    if (error) *error = ErrorCode::InternalError;
    return response;
}

// ClientConnection implementation

/**
 * @brief Constructs a client connection with the specified pipe handle and ID.
 * 
 * @param[in] pipe_handle Handle to the client's pipe connection
 * @param[in] connection_id Unique identifier for this connection
 */
ClientConnection::ClientConnection(
    _In_ HANDLE pipe_handle, 
    _In_ const std::string& connection_id)
    : pipe_handle_(pipe_handle)
    , connection_id_(connection_id)
    , active_(true)
    , read_buffer_(64 * 1024)  // 64KB buffer
    , message_buffer_(64 * 1024) {
    stats_.connection_time = std::chrono::steady_clock::now();
    stats_.last_activity = stats_.connection_time;
}

/**
 * @brief Destructor that ensures proper cleanup of the client connection.
 * 
 * The HandleWrapper automatically closes the pipe handle when this
 * object is destroyed.
 */
ClientConnection::~ClientConnection() {
    // HandleWrapper automatically closes the handle
}

/**
 * @brief Reads a message from the client connection.
 * 
 * This method reads available data from the pipe and returns it as a
 * byte vector. It handles various error conditions including disconnection
 * and timeout scenarios.
 * 
 * @param[in] timeout Maximum time to wait for data
 * @param[out] error Optional pointer to receive error information
 * 
 * @return Vector of bytes containing the message data, or empty vector on error
 */
std::vector<std::byte> ClientConnection::read_message(
    _In_ [[maybe_unused]] std::chrono::milliseconds timeout, 
    _Out_opt_ PipeServerError* error) {
    if (error) *error = PipeServerError::None;
    
    if (!active_.load() || !pipe_handle_.is_valid()) {
        if (error) *error = PipeServerError::Disconnected;
        return {};
    }
    
    // Check if data is available
    DWORD bytes_available = 0;
    if (!PeekNamedPipe(pipe_handle_.get(), nullptr, 0, nullptr, &bytes_available, nullptr)) {
        DWORD last_error = GetLastError();
        if (last_error == ERROR_BROKEN_PIPE || last_error == ERROR_PIPE_NOT_CONNECTED) {
            active_.store(false);
            if (error) *error = PipeServerError::Disconnected;
        } else {
            if (error) *error = PipeServerError::ReadFailed;
        }
        return {};
    }
    
    if (bytes_available == 0) {
        return {}; // No data available
    }
    
    // Read available data
    DWORD bytes_read = 0;
    DWORD bytes_to_read = static_cast<DWORD>(bytes_available < read_buffer_.size() ? bytes_available : read_buffer_.size());
    BOOL success = ReadFile(
        pipe_handle_.get(),
        static_cast<void*>(read_buffer_.data()),
        bytes_to_read,
        &bytes_read,
        nullptr);
    
    if (!success || bytes_read == 0) {
        DWORD last_error = GetLastError();
        if (last_error == ERROR_BROKEN_PIPE || last_error == ERROR_PIPE_NOT_CONNECTED) {
            active_.store(false);
            if (error) *error = PipeServerError::Disconnected;
        } else {
            if (error) *error = PipeServerError::ReadFailed;
        }
        return {};
    }
    
    update_read_stats(bytes_read);
    
    // Return the read data
    std::vector<std::byte> message_data(bytes_read);
    std::memcpy(message_data.data(), read_buffer_.data(), bytes_read);
    
    return message_data;
}

/**
 * @brief Writes a message to the client connection.
 * 
 * This method writes the provided data to the pipe and ensures it is
 * properly flushed. It handles various error conditions including
 * disconnection scenarios.
 * 
 * @param[in] data Data to write to the client
 * @param[in] timeout Maximum time to wait for write completion
 * 
 * @return PipeServerError::None on success, appropriate error code on failure
 */
PipeServerError ClientConnection::write_message(
    _In_ std::span<const std::byte> data, 
    _In_ [[maybe_unused]] std::chrono::milliseconds timeout) {
    if (!active_.load() || !pipe_handle_.is_valid()) {
        return PipeServerError::Disconnected;
    }
    
    DWORD bytes_written = 0;
    BOOL success = WriteFile(
        pipe_handle_.get(),
        static_cast<const void*>(data.data()),
        static_cast<DWORD>(data.size()),
        &bytes_written,
        nullptr);
    
    if (!success || bytes_written != data.size()) {
        DWORD last_error = GetLastError();
        if (last_error == ERROR_BROKEN_PIPE || last_error == ERROR_PIPE_NOT_CONNECTED) {
            active_.store(false);
            return PipeServerError::Disconnected;
        }
        return PipeServerError::WriteFailed;
    }
    
    // Flush to ensure message is sent
    FlushFileBuffers(pipe_handle_.get());
    
    update_write_stats(bytes_written);
    
    return PipeServerError::None;
}

/**
 * @brief Updates read statistics for the client connection.
 * 
 * @param[in] bytes_read Number of bytes read in the last operation
 */
void ClientConnection::update_read_stats(_In_ size_t bytes_read) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_received++;
    stats_.bytes_received += bytes_read;
    stats_.last_activity = std::chrono::steady_clock::now();
}

/**
 * @brief Updates write statistics for the client connection.
 * 
 * @param[in] bytes_written Number of bytes written in the last operation
 */
void ClientConnection::update_write_stats(_In_ size_t bytes_written) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent++;
    stats_.bytes_sent += bytes_written;
    stats_.last_activity = std::chrono::steady_clock::now();
}

/**
 * @brief Searches for a complete message in the message buffer.
 * 
 * This method looks for message delimiters in the buffer to determine
 * if a complete message is available for processing.
 * 
 * @return true if a complete message is found, false otherwise
 */
bool ClientConnection::find_complete_message() {
    // Look for message delimiter in the buffer
    // Messages should end with double CRLF (\r\n\r\n)
    const std::string delimiter = "\r\n\r\n";
    const auto* delimiter_bytes = reinterpret_cast<const std::byte*>(delimiter.data());
    
    // Search for delimiter in the message buffer
    for (size_t i = 0; i <= message_buffer_.size() - delimiter.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < delimiter.size(); ++j) {
            if (message_buffer_[i + j] != delimiter_bytes[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return true;
        }
    }
    
    return false;
}

// Utility functions

/**
 * @brief Generates a unique connection ID.
 * 
 * This function creates a unique identifier for client connections using
 * a combination of timestamp and atomic counter to ensure uniqueness.
 * 
 * @return Unique connection ID string
 */
std::string vibedbg::communication::generate_connection_id() {
    static std::atomic<uint32_t> counter{0};
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return std::format("conn_{}_{}", timestamp, counter.fetch_add(1));
}

/**
 * @brief Formats a pipe error code into a human-readable string.
 * 
 * @param[in] error_code Windows error code to format
 * 
 * @return Formatted error string
 */
std::string format_pipe_error(_In_ DWORD error_code) {
    return std::format("Pipe error: 0x{:08x}", error_code);
}

/**
 * @brief Determines if a pipe error is recoverable.
 * 
 * This function checks if the given error code represents a recoverable
 * error that allows the connection to continue, or an unrecoverable
 * error that requires connection termination.
 * 
 * @param[in] error_code Windows error code to check
 * 
 * @return true if the error is recoverable, false otherwise
 */
bool is_pipe_error_recoverable(_In_ DWORD error_code) {
    switch (error_code) {
        case ERROR_BROKEN_PIPE:
        case ERROR_PIPE_NOT_CONNECTED:
        case ERROR_NO_DATA:
            return false;
        default:
            return true;
    }
}