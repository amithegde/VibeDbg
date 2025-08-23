"""
Configuration management for VibeDbg MCP Server.

This module contains all configuration constants, timeouts, and settings
used throughout the application to ensure consistency and easy maintenance.
"""

import os
import logging
from typing import Optional
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger(__name__)

# ====================================================================
# COMMUNICATION SETTINGS
# ====================================================================

# Named pipe configuration
DEFAULT_PIPE_NAME = r"\\.\pipe\vibedbg_debug"
DEFAULT_BUFFER_SIZE = 8192

# Basic timeout settings (in milliseconds)
DEFAULT_TIMEOUT_MS = 30000
QUICK_COMMAND_TIMEOUT_MS = 10000
LONG_COMMAND_TIMEOUT_MS = 120000

# Enhanced timeout settings for large operations
BULK_OPERATION_TIMEOUT_MS = 180000
LARGE_ANALYSIS_TIMEOUT_MS = 300000
PROCESS_LIST_TIMEOUT_MS = 480000
STREAMING_TIMEOUT_MS = 900000

# Retry configuration
MAX_RETRY_ATTEMPTS = 3
RETRY_DELAY_MS = 1000
NETWORK_DEBUGGING_TIMEOUT_MULTIPLIER = 2.0

# ====================================================================
# MONITORING SETTINGS
# ====================================================================

# Health monitoring intervals (in seconds)
CONNECTION_HEALTH_CHECK_INTERVAL = 30
ASYNC_MONITORING_INTERVAL = 30
SESSION_RECOVERY_CHECK_INTERVAL = 60

# Monitoring thresholds
LOW_SUCCESS_RATE_THRESHOLD = 0.5
WARNING_SUCCESS_RATE_THRESHOLD = 0.8
HIGH_EXECUTION_TIME_THRESHOLD = 10.0
HIGH_PENDING_TASKS_THRESHOLD = 10

# ====================================================================
# OUTPUT FORMATTING SETTINGS
# ====================================================================

# Output formatting configuration
INCLUDE_COMMAND_CONTEXT_IN_ERRORS = True
INCLUDE_COMMAND_CONTEXT_IN_SEQUENCES = True
INCLUDE_COMMAND_CONTEXT_IN_COMPLEX_COMMANDS = True

# Commands that benefit from context inclusion
COMPLEX_COMMANDS_THAT_NEED_CONTEXT = {
    "da",
    "db",
    "dq",
    "dps",
    "u",
    "uf",
    "k",
    "kn",
    "lm",
    "x",
    "ln",
    "dx",
    "!process",
    "!thread",
    "!peb",
    "!teb",
    "!analyze",
    "!crash",
}

# ====================================================================
# PERFORMANCE SETTINGS
# ====================================================================

# Cache configuration
DEFAULT_CACHE_SIZE = 100
MAX_CACHE_AGE_HOURS = 1
CACHE_CLEANUP_INTERVAL = 300  # 5 minutes

# Async operation limits
MAX_CONCURRENT_OPERATIONS = 5
TASK_CLEANUP_INTERVAL_HOURS = 1
MAX_ASYNC_HISTORY_SIZE = 100

# ====================================================================
# SESSION RECOVERY SETTINGS
# ====================================================================

# Recovery configuration
MAX_RECOVERY_ATTEMPTS = 3
SESSION_SNAPSHOT_INTERVAL = 300  # 5 minutes
SESSION_STATE_FILE = "vibedbg_session_state.json"

# DebuggingMode removed - we only support user-mode debugging


class OptimizationLevel(Enum):
    """Performance optimization level."""

    DISABLED = "disabled"
    BASIC = "basic"
    MODERATE = "moderate"
    AGGRESSIVE = "aggressive"


# ====================================================================
# TIMEOUT CONFIGURATIONS
# ====================================================================


@dataclass
class TimeoutConfig:
    """Timeout configuration for different command types."""

    quick: int = 10000  # Quick commands (version, help, etc.)
    normal: int = 30000  # Normal commands
    analysis: int = 120000  # Analysis commands
    bulk: int = 180000  # Bulk operations
    large: int = 300000  # Large analysis operations
    streaming: int = 900000  # Streaming operations


# ====================================================================
# MAIN CONFIGURATION CLASS
# ====================================================================


@dataclass
class ServerConfig:
    """Server configuration settings."""

    # Named pipe settings
    pipe_name: str = DEFAULT_PIPE_NAME
    buffer_size: int = DEFAULT_BUFFER_SIZE
    connection_timeout_ms: int = DEFAULT_TIMEOUT_MS
    command_timeout_ms: int = DEFAULT_TIMEOUT_MS

    # Server settings
    max_connections: int = 10
    enable_heartbeat: bool = True
    heartbeat_interval_ms: int = 10000

    # Command execution settings
    enable_command_validation: bool = True
    enable_command_caching: bool = True
    cache_ttl_seconds: int = 300
    max_retry_attempts: int = MAX_RETRY_ATTEMPTS
    retry_delay_ms: int = RETRY_DELAY_MS

    # Logging settings
    log_level: str = "INFO"
    enable_debug_logging: bool = False
    log_format: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    # Performance settings
    enable_async_execution: bool = True
    max_concurrent_commands: int = MAX_CONCURRENT_OPERATIONS
    optimization_level: OptimizationLevel = OptimizationLevel.MODERATE

    # Monitoring settings
    enable_health_monitoring: bool = True
    health_check_interval: int = CONNECTION_HEALTH_CHECK_INTERVAL

    # Session recovery settings
    enable_session_recovery: bool = True
    max_recovery_attempts: int = MAX_RECOVERY_ATTEMPTS

    # Timeout configurations
    timeouts: TimeoutConfig = None

    def __post_init__(self):
        if self.timeouts is None:
            self.timeouts = TimeoutConfig()

    @classmethod
    def from_environment(cls) -> "ServerConfig":
        """Create configuration from environment variables."""
        try:
            return cls(
                pipe_name=os.getenv("VIBEDBG_PIPE_NAME", DEFAULT_PIPE_NAME),
                buffer_size=int(
                    os.getenv("VIBEDBG_BUFFER_SIZE", str(DEFAULT_BUFFER_SIZE))
                ),
                connection_timeout_ms=int(
                    os.getenv("VIBEDBG_CONNECTION_TIMEOUT_MS", str(DEFAULT_TIMEOUT_MS))
                ),
                command_timeout_ms=int(
                    os.getenv("VIBEDBG_COMMAND_TIMEOUT_MS", str(DEFAULT_TIMEOUT_MS))
                ),
                max_connections=int(os.getenv("VIBEDBG_MAX_CONNECTIONS", "10")),
                enable_heartbeat=os.getenv("VIBEDBG_ENABLE_HEARTBEAT", "true").lower()
                == "true",
                heartbeat_interval_ms=int(
                    os.getenv("VIBEDBG_HEARTBEAT_INTERVAL_MS", "10000")
                ),
                enable_command_validation=os.getenv(
                    "VIBEDBG_ENABLE_VALIDATION", "true"
                ).lower()
                == "true",
                enable_command_caching=os.getenv(
                    "VIBEDBG_ENABLE_CACHING", "true"
                ).lower()
                == "true",
                cache_ttl_seconds=int(os.getenv("VIBEDBG_CACHE_TTL_SECONDS", "300")),
                max_retry_attempts=int(
                    os.getenv("VIBEDBG_MAX_RETRY_ATTEMPTS", str(MAX_RETRY_ATTEMPTS))
                ),
                retry_delay_ms=int(
                    os.getenv("VIBEDBG_RETRY_DELAY_MS", str(RETRY_DELAY_MS))
                ),
                log_level=os.getenv("VIBEDBG_LOG_LEVEL", "INFO"),
                enable_debug_logging=os.getenv("VIBEDBG_DEBUG_LOGGING", "false").lower()
                == "true",
                enable_async_execution=os.getenv(
                    "VIBEDBG_ASYNC_EXECUTION", "true"
                ).lower()
                == "true",
                max_concurrent_commands=int(
                    os.getenv(
                        "VIBEDBG_MAX_CONCURRENT_COMMANDS",
                        str(MAX_CONCURRENT_OPERATIONS),
                    )
                ),
                optimization_level=OptimizationLevel(
                    os.getenv("VIBEDBG_OPTIMIZATION_LEVEL", "moderate")
                ),
                enable_health_monitoring=os.getenv(
                    "VIBEDBG_HEALTH_MONITORING", "true"
                ).lower()
                == "true",
                health_check_interval=int(
                    os.getenv(
                        "VIBEDBG_HEALTH_CHECK_INTERVAL",
                        str(CONNECTION_HEALTH_CHECK_INTERVAL),
                    )
                ),
                enable_session_recovery=os.getenv(
                    "VIBEDBG_SESSION_RECOVERY", "true"
                ).lower()
                == "true",
                max_recovery_attempts=int(
                    os.getenv(
                        "VIBEDBG_MAX_RECOVERY_ATTEMPTS", str(MAX_RECOVERY_ATTEMPTS)
                    )
                ),
            )
        except (ValueError, TypeError) as e:
            logger.error(f"Failed to parse environment configuration: {e}")
            # Return default configuration on error
            return cls()


# ====================================================================
# UTILITY FUNCTIONS
# ====================================================================


def get_timeout_for_command(command: str) -> int:
    """
    Get appropriate timeout for a specific command type.

    Args:
        command: WinDbg command to get timeout for

    Returns:
        Timeout in milliseconds
    """
    try:
        command_lower = command.lower().strip()

        # Quick commands
        if command_lower in ["version", "help", "?", "cls", "clear"]:
            return QUICK_COMMAND_TIMEOUT_MS

        # Analysis commands
        if any(
            keyword in command_lower
            for keyword in ["!analyze", "!dump", "!process", "!thread"]
        ):
            return LARGE_ANALYSIS_TIMEOUT_MS

        # Bulk operations
        if any(
            keyword in command_lower
            for keyword in ["!process 0 0", "!thread 0 0", "lm"]
        ):
            return BULK_OPERATION_TIMEOUT_MS

        # Streaming operations
        if any(keyword in command_lower for keyword in ["!for_each", "!foreach"]):
            return STREAMING_TIMEOUT_MS

        # Default timeout
        return DEFAULT_TIMEOUT_MS
    except Exception as e:
        logger.warning(f"Error determining timeout for command '{command}': {e}")
        return DEFAULT_TIMEOUT_MS


# Global configuration instance
try:
    config = ServerConfig.from_environment()
    logger.info("Configuration loaded successfully from environment")
except Exception as e:
    logger.error(f"Failed to load configuration from environment: {e}")
    config = ServerConfig()

# Logging configuration
LOG_LEVEL = config.log_level
DEBUG_ENABLED = config.enable_debug_logging
LOG_FORMAT = config.log_format
