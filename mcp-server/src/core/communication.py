"""
Communication layer for VibeDbg MCP Server.

This module handles all communication with the WinDbg extension via named pipes,
including message serialization, connection management, and error handling.
"""

import json
import logging
import time
import threading
from typing import Optional, Dict, Any, List
from dataclasses import dataclass
from datetime import datetime
from contextlib import contextmanager
import win32pipe
import win32file
import win32api
import win32event
import pywintypes

from ..config import (
    config,
    DEFAULT_TIMEOUT_MS,
    get_timeout_for_command,
    MAX_RETRY_ATTEMPTS,
    RETRY_DELAY_MS,
)
from ..protocol.messages import (
    CommandRequest,
    CommandResponse,
    ErrorMessage,
    HeartbeatMessage,
    create_command_request,
    create_response,
    create_error,
)

logger = logging.getLogger(__name__)

# ====================================================================
# EXCEPTION CLASSES
# ====================================================================


class CommunicationError(Exception):
    """Base exception for communication errors."""

    pass


class PipeTimeoutError(CommunicationError):
    """Raised when a command times out."""

    pass


class ConnectionError(CommunicationError):
    """Raised when connection to WinDbg extension fails."""

    pass


class NetworkDebuggingError(CommunicationError):
    """Raised when network debugging connection issues are detected."""

    pass


# Backward compatibility alias
TimeoutError = PipeTimeoutError

# ====================================================================
# DATA CLASSES
# ====================================================================


@dataclass
class ConnectionHandle:
    """Represents a connection handle with metadata."""

    handle: Any
    created_at: datetime
    last_used: datetime
    in_use: bool = False
    use_count: int = 0
    thread_id: int = 0


@dataclass
class ConnectionHealth:
    """Represents the health status of the WinDbg connection."""

    is_connected: bool
    last_successful_command: Optional[datetime]
    consecutive_failures: int
    target_responsive: bool
    extension_responsive: bool
    last_error: Optional[str]


# ====================================================================
# LOW-LEVEL PROTOCOL CLASSES
# ====================================================================


class NamedPipeProtocol:
    """Handles low-level named pipe communication protocol."""

    @staticmethod
    def connect_to_pipe(pipe_name: str, timeout_ms: int) -> Any:
        """
        Connect to the WinDbg extension named pipe.

        Args:
            pipe_name: Name of the pipe to connect to
            timeout_ms: Connection timeout in milliseconds

        Returns:
            Handle to the connected pipe

        Raises:
            ConnectionError: If connection fails
        """
        try:
            handle = win32file.CreateFile(
                pipe_name,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None,
            )
            logger.debug(f"Connected to pipe: {pipe_name}")
            return handle

        except pywintypes.error as e:
            error_code = e.args[0]

            if error_code == 2:  # ERROR_FILE_NOT_FOUND
                raise ConnectionError(
                    "WinDbg extension not found. Make sure the extension is loaded in WinDbg."
                )
            elif error_code == 231:  # ERROR_PIPE_BUSY
                # Wait for pipe to become available
                start_time = time.time()
                while (time.time() - start_time) * 1000 < timeout_ms:
                    if win32pipe.WaitNamedPipe(pipe_name, min(5000, timeout_ms)):
                        try:
                            handle = win32file.CreateFile(
                                pipe_name,
                                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                                0,
                                None,
                                win32file.OPEN_EXISTING,
                                0,
                                None,
                            )
                            logger.debug(
                                f"Connected to pipe after waiting: {pipe_name}"
                            )
                            return handle
                        except pywintypes.error as retry_error:
                            if retry_error.args[0] != 231:
                                raise ConnectionError(
                                    f"Failed to connect after wait: {str(retry_error)}"
                                )
                            time.sleep(0.1)
                    else:
                        time.sleep(0.1)

                raise ConnectionError("WinDbg extension is busy and timeout exceeded.")
            else:
                raise ConnectionError(
                    f"Failed to connect to WinDbg extension: {str(e)}"
                )

    @staticmethod
    def write_to_pipe(handle: Any, data: bytes, timeout_ms: int):
        """Write data to the pipe with better error handling."""
        try:
            win32file.WriteFile(handle, data)
            logger.debug(f"Successfully wrote {len(data)} bytes to pipe")
        except pywintypes.error as e:
            error_code = e.args[0]
            if error_code == 232:  # ERROR_NO_DATA - pipe being closed
                raise ConnectionError(f"Failed to write to pipe: {str(e)}")
            elif error_code == 109:  # ERROR_BROKEN_PIPE
                raise ConnectionError(f"Pipe connection broken: {str(e)}")
            elif error_code == 231:  # ERROR_PIPE_BUSY
                raise ConnectionError(f"Pipe is busy: {str(e)}")
            else:
                raise ConnectionError(f"Failed to write to pipe: {str(e)}")

    @staticmethod
    def read_from_pipe(handle: Any, timeout_ms: int) -> bytes:
        """Read response from the pipe."""
        start_time = datetime.now()
        response_data = b""

        while True:
            elapsed_ms = int((datetime.now() - start_time).total_seconds() * 1000)
            if elapsed_ms > timeout_ms:
                raise TimeoutError(f"Read operation timed out after {elapsed_ms}ms")

            try:
                hr, data = win32file.ReadFile(handle, config.buffer_size)

                if data:
                    response_data += data
                    logger.debug(
                        f"Read {len(data)} bytes, total: {len(response_data)} bytes"
                    )

                    # Check for message delimiter
                    if b"\r\n\r\n" in response_data:
                        logger.debug("Found complete response")
                        break
                else:
                    time.sleep(0.01)

            except pywintypes.error as e:
                error_code = e.args[0]
                if error_code == 109:  # ERROR_BROKEN_PIPE
                    if response_data:
                        logger.warning("Pipe broken but have partial data, using it")
                        break
                    raise ConnectionError("Pipe connection broken")
                elif error_code == 232:  # ERROR_NO_DATA
                    time.sleep(0.01)
                    continue
                else:
                    raise ConnectionError(f"Failed to read from pipe: {str(e)}")

        logger.debug(f"Successfully read complete response: {len(response_data)} bytes")
        return response_data

    @staticmethod
    def close_pipe(handle: Any):
        """Safely close a pipe handle."""
        if handle:
            try:
                win32file.CloseHandle(handle)
            except Exception as e:
                logger.warning(f"Error closing pipe handle: {e}")


# ====================================================================
# MESSAGE PROTOCOL ADAPTER
# ====================================================================


class MessageProtocolAdapter:
    """Adapter for the existing message protocol to work with the communication layer."""

    @staticmethod
    def create_command_message(command: str, timeout_ms: int) -> Dict[str, Any]:
        """Create a command message compatible with the WinDbg extension."""
        # Use the proper protocol format that matches C++ extension
        return {
            "protocol_version": 1,
            "message_type": 1,  # Command type
            "payload": {
                "type": "command",
                "request_id": str(int(time.time() * 1000)),
                "command": command,
                "parameters": {},
                "timeout_ms": timeout_ms,
                "timestamp": int(time.time() * 1000),
            },
        }

    @staticmethod
    def create_handler_message(handler_name: str, **kwargs) -> Dict[str, Any]:
        """Create a direct handler message for the WinDbg extension."""
        # Use the proper protocol format that matches C++ extension
        return {
            "protocol_version": 1,
            "message_type": 1,  # Command type
            "payload": {
                "type": "command",
                "request_id": str(int(time.time() * 1000)),
                "command": handler_name,
                "parameters": kwargs or {},
                "timeout_ms": 30000,
                "timestamp": int(time.time() * 1000),
            },
        }

    @staticmethod
    def serialize_message(message: Dict[str, Any]) -> bytes:
        """Serialize a message to bytes for transmission."""
        try:
            # Use the same format as C++ extension
            message_str = json.dumps(message, separators=(",", ":"))
            message_bytes = message_str.encode("utf-8")

            # Add delimiter to match C++ extension
            delimiter = b"\r\n\r\n"
            return message_bytes + delimiter

        except (TypeError, ValueError) as e:
            raise CommunicationError(f"Failed to serialize message: {e}")

    @staticmethod
    def parse_response(response_data: bytes) -> Dict[str, Any]:
        """Parse the response data from the extension."""
        try:
            # Remove delimiter if present
            if response_data.endswith(b"\r\n\r\n"):
                response_data = response_data[:-4]

            # Decode and parse JSON
            response_str = response_data.decode("utf-8")
            parsed = json.loads(response_str)

            # Handle the C++ extension response format
            if "protocol_version" in parsed and "payload" in parsed:
                # This is the new protocol format
                payload = parsed["payload"]

                if payload.get("type") == "response":
                    return {
                        "status": (
                            "success" if payload.get("success", False) else "error"
                        ),
                        "output": payload.get("output", ""),
                        "error": (
                            payload.get("error_message", "")
                            if not payload.get("success", False)
                            else None
                        ),
                        "execution_time_ms": payload.get("execution_time_ms", 0),
                        "request_id": payload.get("request_id", ""),
                    }
                elif payload.get("type") == "error":
                    return {
                        "status": "error",
                        "error": payload.get("error_message", "Unknown error"),
                        "error_code": payload.get("error_code", 0),
                        "request_id": payload.get("request_id", ""),
                    }
            else:
                # Fallback to old format
                return parsed

        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse response: {e}")
            logger.debug(f"Raw response: {response_data!r}")
            raise CommunicationError(f"Invalid response from WinDbg extension")
        except UnicodeDecodeError as e:
            logger.error(f"Failed to decode response: {e}")
            raise CommunicationError(f"Invalid response encoding from WinDbg extension")

    @staticmethod
    def validate_response(response: Dict[str, Any]) -> bool:
        """Validate that a response message has the expected structure."""
        try:
            if "status" not in response:
                return False

            if response["status"] == "error" and "error" not in response:
                return False

            if response["status"] == "success" and "output" not in response:
                return False

            return True
        except Exception:
            return False

    @staticmethod
    def detect_network_debugging_error(error_message: str) -> bool:
        """Detect if an error is related to network debugging connection issues."""
        network_error_indicators = [
            "network",
            "connection",
            "timeout",
            "unreachable",
            "refused",
            "ERROR_BROKEN_PIPE",
            "ERROR_PIPE_BUSY",
            "ERROR_PIPE_NOT_CONNECTED",
            "extension not initialized",  # Add this as it indicates connection issues
        ]

        error_lower = error_message.lower()
        return any(indicator in error_lower for indicator in network_error_indicators)


# ====================================================================
# CONNECTION POOL MANAGEMENT
# ====================================================================


class ConnectionPool:
    """Thread-safe connection pool for named pipe connections."""

    def __init__(self, max_connections: int = 3):
        self._max_connections = max_connections
        self._connections: List[ConnectionHandle] = []
        self._lock = threading.RLock()
        self._pipe_name = config.pipe_name
        self._active_requests = 0
        self._max_concurrent_requests = 10
        self._queue_condition = threading.Condition(self._lock)

    @contextmanager
    def get_connection(self, timeout_ms: int = DEFAULT_TIMEOUT_MS):
        """Context manager to get a connection from the pool."""
        connection = None
        try:
            connection = self._acquire_connection(timeout_ms)
            yield connection
        finally:
            if connection:
                self._release_connection(connection)

    def _acquire_connection(self, timeout_ms: int) -> Any:
        """Acquire a connection from the pool."""
        try:
            with self._lock:
                # Wait for available connection
                start_time = time.time()
                while (time.time() - start_time) * 1000 < timeout_ms:
                    # Check for available connection
                    for conn_handle in self._connections:
                        if not conn_handle.in_use:
                            conn_handle.in_use = True
                            conn_handle.last_used = datetime.now()
                            conn_handle.use_count += 1
                            conn_handle.thread_id = threading.get_ident()
                            logger.debug("Reused existing connection from pool")
                            return conn_handle.handle

                    # Create new connection if pool not full
                    if len(self._connections) < self._max_connections:
                        try:
                            handle = NamedPipeProtocol.connect_to_pipe(
                                self._pipe_name, timeout_ms
                            )
                            conn_handle = ConnectionHandle(
                                handle=handle,
                                created_at=datetime.now(),
                                last_used=datetime.now(),
                                in_use=True,
                                use_count=1,
                                thread_id=threading.get_ident(),
                            )
                            self._connections.append(conn_handle)
                            logger.debug("Created new connection for pool")
                            return handle
                        except ConnectionError as ce:
                            logger.warning(
                                f"Failed to create new connection (connection error): {ce}"
                            )
                            # Continue waiting for existing connections to become available
                        except Exception as e:
                            logger.error(
                                f"Unexpected error creating new connection: {e}",
                                exc_info=True,
                            )
                            # Continue waiting for existing connections to become available

                    # Wait for connection to become available
                    try:
                        self._queue_condition.wait(0.1)
                    except Exception as e:
                        logger.error(
                            f"Error waiting for connection availability: {e}",
                            exc_info=True,
                        )
                        break

                raise ConnectionError("Timeout waiting for available connection")
        except ConnectionError:
            raise
        except Exception as e:
            logger.error(f"Unexpected error acquiring connection: {e}", exc_info=True)
            raise ConnectionError(f"Failed to acquire connection: {e}")

    def _release_connection(self, connection: Any):
        """Release a connection back to the pool."""
        try:
            with self._lock:
                for conn_handle in self._connections:
                    if conn_handle.handle == connection:
                        conn_handle.in_use = False
                        conn_handle.last_used = datetime.now()
                        try:
                            self._queue_condition.notify()
                        except Exception as e:
                            logger.warning(
                                f"Error notifying condition during connection release: {e}"
                            )
                        logger.debug("Released connection back to pool")
                        break
        except Exception as e:
            logger.error(f"Error releasing connection: {e}", exc_info=True)

    def cleanup_old_connections(self, max_age_hours: int = 1):
        """Clean up old connections from the pool."""
        try:
            with self._lock:
                current_time = datetime.now()
                connections_to_remove = []

                for conn_handle in self._connections:
                    try:
                        if not conn_handle.in_use:
                            age_hours = (
                                current_time - conn_handle.created_at
                            ).total_seconds() / 3600
                            if age_hours > max_age_hours:
                                connections_to_remove.append(conn_handle)
                    except Exception as e:
                        logger.warning(f"Error checking connection age: {e}")
                        # Add problematic connection to removal list
                        connections_to_remove.append(conn_handle)

                for conn_handle in connections_to_remove:
                    try:
                        NamedPipeProtocol.close_pipe(conn_handle.handle)
                        self._connections.remove(conn_handle)
                        logger.debug("Cleaned up old connection from pool")
                    except ValueError:
                        # Connection already removed
                        logger.debug("Connection already removed during cleanup")
                    except Exception as e:
                        logger.error(
                            f"Error cleaning up connection: {e}", exc_info=True
                        )
        except Exception as e:
            logger.error(f"Error during connection cleanup: {e}", exc_info=True)

    def close_all_connections(self):
        """Close all connections in the pool."""
        try:
            with self._lock:
                for conn_handle in self._connections:
                    try:
                        NamedPipeProtocol.close_pipe(conn_handle.handle)
                    except Exception as e:
                        logger.error(f"Error closing connection: {e}", exc_info=True)
                self._connections.clear()
                logger.debug("All connections closed and pool cleared")
        except Exception as e:
            logger.error(f"Error during close all connections: {e}", exc_info=True)


# ====================================================================
# HIGH-LEVEL COMMUNICATION MANAGER
# ====================================================================


class CommunicationManager:
    """Main communication manager for WinDbg extension interaction."""

    def __init__(self):
        self._connection_health = ConnectionHealth(
            is_connected=True,
            last_successful_command=None,
            consecutive_failures=0,
            target_responsive=True,
            extension_responsive=True,
            last_error=None,
        )
        self._health_lock = threading.Lock()
        self._connection_pool = ConnectionPool()
        self._retry_count = 0

    def send_command(self, command: str, timeout_ms: int = DEFAULT_TIMEOUT_MS) -> str:
        """Send a command to the WinDbg extension."""
        logger.debug(f"Sending command: {command}")

        message = MessageProtocolAdapter.create_command_message(command, timeout_ms)

        try:
            response = self._send_message(message, timeout_ms)

            if not MessageProtocolAdapter.validate_response(response):
                raise CommunicationError(
                    "Invalid response structure from WinDbg extension"
                )

            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")

                if MessageProtocolAdapter.detect_network_debugging_error(error_message):
                    raise NetworkDebuggingError(
                        f"Network debugging connection issue: {error_message}"
                    )

                # Handle extension not initialized - this can happen after .restart
                if "extension not initialized" in error_message.lower():
                    logger.warning(
                        f"Extension not initialized, may need reconnection: {error_message}"
                    )
                    # This will trigger the reconnection logic in command_executor
                    raise ConnectionError(f"Extension not initialized: {error_message}")

                raise CommunicationError(f"WinDbg command failed: {error_message}")

            self._update_health_on_success()
            return response.get("output", "")

        except (TimeoutError, ConnectionError, NetworkDebuggingError):
            self._update_health_on_failure(f"Command '{command}' failed")
            raise
        except ValueError as e:
            self._update_health_on_failure(f"Invalid command '{command}': {str(e)}")
            logger.error(f"Invalid command '{command}': {e}")
            raise CommunicationError(f"Invalid command: {str(e)}")
        except Exception as e:
            self._update_health_on_failure(str(e))
            logger.error(
                f"Unexpected error executing command '{command}': {e}", exc_info=True
            )
            raise CommunicationError(f"Unexpected error executing command: {str(e)}")

    def send_handler_command(
        self, handler_name: str, timeout_ms: int = DEFAULT_TIMEOUT_MS, **kwargs
    ) -> Dict[str, Any]:
        """Send a direct handler command to the WinDbg extension."""
        logger.debug(f"Sending handler command: {handler_name}")

        message = MessageProtocolAdapter.create_handler_message(handler_name, **kwargs)

        try:
            response = self._send_message(message, timeout_ms)

            if not isinstance(response, dict):
                raise CommunicationError(
                    "Invalid response structure from WinDbg extension handler"
                )

            if response.get("type") == "error":
                error_message = response.get("error_message", "Unknown error")
                raise CommunicationError(
                    f"Handler '{handler_name}' failed: {error_message}"
                )

            self._update_health_on_success()
            return response

        except (TimeoutError, ConnectionError):
            self._update_health_on_failure(f"Handler '{handler_name}' failed")
            raise
        except ValueError as e:
            self._update_health_on_failure(
                f"Invalid handler call '{handler_name}': {str(e)}"
            )
            logger.error(f"Invalid handler call '{handler_name}': {e}")
            raise CommunicationError(f"Invalid handler call: {str(e)}")
        except Exception as e:
            self._update_health_on_failure(str(e))
            logger.error(
                f"Unexpected error executing handler '{handler_name}': {e}",
                exc_info=True,
            )
            raise CommunicationError(f"Unexpected error executing handler: {str(e)}")

    def _send_message(self, message: Dict[str, Any], timeout_ms: int) -> Dict[str, Any]:
        """Send a message and receive response with improved retry logic."""
        last_exception = None
        max_attempts = min(
            config.max_retry_attempts + 1, 4
        )  # Cap retries for faster failure

        for attempt in range(max_attempts):
            try:
                with self._connection_pool.get_connection(timeout_ms) as connection:
                    # Serialize and send message
                    message_data = MessageProtocolAdapter.serialize_message(message)
                    NamedPipeProtocol.write_to_pipe(
                        connection, message_data, timeout_ms
                    )

                    # Read response
                    response_data = NamedPipeProtocol.read_from_pipe(
                        connection, timeout_ms
                    )

                    # Parse response
                    response = MessageProtocolAdapter.parse_response(response_data)
                    return response

            except (TimeoutError, ConnectionError) as e:
                last_exception = e
                error_str = str(e)

                # Check if it's a pipe being closed error - try to recover by clearing connection pool
                if "pipe is being closed" in error_str.lower() or "232" in error_str:
                    logger.warning(
                        f"Pipe closing detected on attempt {attempt + 1}: {e}"
                    )

                    # Clear connection pool to force new connections
                    self._connection_pool.close_all_connections()

                    if attempt < max_attempts - 1:
                        logger.info(
                            f"Clearing connection pool and retrying (attempt {attempt + 2})..."
                        )
                        time.sleep(0.5)  # Brief pause before retry
                        continue
                    else:
                        logger.error(
                            f"Pipe connection failed after {max_attempts} attempts"
                        )
                        raise

                if attempt < max_attempts - 1:
                    # Reduce retry delay for faster recovery
                    retry_delay = min(config.retry_delay_ms / 1000.0, 0.5)
                    logger.warning(f"Attempt {attempt + 1} failed: {e}, retrying...")
                    time.sleep(retry_delay)
                else:
                    logger.error(f"All {max_attempts} attempts failed")
                    raise
            except ValueError as e:
                # Don't retry on validation errors
                logger.error(f"Validation error on attempt {attempt + 1}: {e}")
                last_exception = e
                break
            except Exception as e:
                last_exception = e
                logger.error(
                    f"Unexpected error on attempt {attempt + 1}: {e}", exc_info=True
                )
                if attempt < max_attempts - 1:
                    time.sleep(0.5)  # Shorter delay for unexpected errors
                else:
                    raise

        raise last_exception or CommunicationError("Failed to send message")

    def test_connection(self) -> bool:
        """Test if the connection to the WinDbg extension is working."""
        try:
            result = self.send_handler_command(
                "version", timeout_ms=get_timeout_for_command("version")
            )
            extension_responsive = bool(result.get("status") == "success")

            with self._health_lock:
                self._connection_health.extension_responsive = extension_responsive
                self._connection_health.is_connected = extension_responsive
                if extension_responsive:
                    self._connection_health.last_successful_command = datetime.now()
                    self._connection_health.consecutive_failures = 0
                    self._connection_health.last_error = None
                else:
                    self._connection_health.consecutive_failures += 1

            return extension_responsive

        except NetworkDebuggingError:
            logger.debug(
                "Network debugging error during connection test - assuming connected"
            )
            with self._health_lock:
                self._connection_health.extension_responsive = True
                self._connection_health.is_connected = True
                self._connection_health.last_successful_command = datetime.now()
                self._connection_health.consecutive_failures = 0
                self._connection_health.last_error = None
            return True

    def _update_health_on_success(self):
        """Update health status on successful command."""
        with self._health_lock:
            self._connection_health.last_successful_command = datetime.now()
            self._connection_health.consecutive_failures = 0
            self._connection_health.last_error = None
            self._connection_health.extension_responsive = True
            self._connection_health.is_connected = True

    def _update_health_on_failure(self, error_message: str):
        """Update health status on command failure."""
        with self._health_lock:
            self._connection_health.consecutive_failures += 1
            self._connection_health.last_error = error_message
            self._connection_health.last_successful_command = None

    def get_connection_health(self) -> ConnectionHealth:
        """Get the current connection health status."""
        with self._health_lock:
            return ConnectionHealth(
                is_connected=self._connection_health.is_connected,
                last_successful_command=self._connection_health.last_successful_command,
                consecutive_failures=self._connection_health.consecutive_failures,
                target_responsive=self._connection_health.target_responsive,
                extension_responsive=self._connection_health.extension_responsive,
                last_error=self._connection_health.last_error,
            )

    def force_reconnect(self) -> bool:
        """Force a reconnection by clearing the connection pool and testing connection."""
        logger.info("Forcing reconnection to WinDbg extension...")
        try:
            # Clear all existing connections
            self._connection_pool.close_all_connections()

            # Reset consecutive failures
            with self._health_lock:
                self._connection_health.consecutive_failures = 0
                self._connection_health.last_error = None

            # Test if we can connect
            return self.test_connection()
        except Exception as e:
            logger.error(f"Failed to force reconnection: {e}")
            return False

    def cleanup(self):
        """Clean up resources."""
        try:
            self._connection_pool.close_all_connections()
            logger.info("Communication manager cleanup completed")
        except Exception as e:
            logger.error(
                f"Error during communication manager cleanup: {e}", exc_info=True
            )
