"""
Message protocol definitions for VibeDbg communication.

This module defines the message types and structures used for communication
between the MCP server and the WinDbg extension via named pipes.
"""

import logging
from enum import Enum
from typing import Dict, Any, Optional
from datetime import datetime, timezone
from pydantic import BaseModel, Field, field_validator, ConfigDict
import uuid

logger = logging.getLogger(__name__)


class MessageType(str, Enum):
    """Message type enumeration."""

    COMMAND = "command"
    RESPONSE = "response"
    ERROR = "error"
    HEARTBEAT = "heartbeat"


class ErrorCode(str, Enum):
    """Error code enumeration."""

    NONE = "none"
    INVALID_MESSAGE = "invalid_message"
    COMMAND_FAILED = "command_failed"
    TIMEOUT = "timeout"
    CONNECTION_LOST = "connection_lost"
    INVALID_PARAMETER = "invalid_parameter"
    UNKNOWN_COMMAND = "unknown_command"
    DEBUGGER_NOT_ATTACHED = "debugger_not_attached"
    INTERNAL_ERROR = "internal_error"


class BaseMessage(BaseModel):
    """Base message class with common fields."""

    type: MessageType
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))

    model_config = ConfigDict(
        use_enum_values=True, json_encoders={datetime: lambda v: v.isoformat()}
    )


class CommandRequest(BaseMessage):
    """Command request message sent from MCP server to WinDbg extension."""

    type: MessageType = MessageType.COMMAND
    request_id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    command: str = Field(..., min_length=1, description="WinDbg command to execute")
    parameters: Dict[str, Any] = Field(default_factory=dict)
    timeout_ms: int = Field(
        default=30000, ge=1000, le=300000, description="Timeout in milliseconds"
    )

    @field_validator("command")
    @classmethod
    def validate_command(cls, v):
        """Validate that command is not empty after stripping."""
        try:
            if not v.strip():
                raise ValueError("Command cannot be empty")
            return v.strip()
        except Exception as e:
            logger.error(f"Command validation failed: {e}")
            raise

    @field_validator("parameters")
    @classmethod
    def validate_parameters(cls, v):
        """Ensure parameters is a dictionary."""
        try:
            if v is None:
                return {}
            return v
        except Exception as e:
            logger.error(f"Parameters validation failed: {e}")
            return {}


class CommandResponse(BaseMessage):
    """Response message from WinDbg extension to MCP server."""

    type: MessageType = MessageType.RESPONSE
    request_id: str = Field(..., description="Matching request ID")
    success: bool = Field(..., description="Whether command execution was successful")
    output: str = Field(default="", description="Command output")
    error_message: str = Field(default="", description="Error message if failed")
    execution_time_ms: int = Field(
        default=0, ge=0, description="Execution time in milliseconds"
    )
    session_data: Optional[Dict[str, Any]] = Field(
        default=None, description="Current session state"
    )
    command_executed: str = Field(
        default="", description="Actual command executed (may be adapted)"
    )

    @field_validator("session_data")
    @classmethod
    def validate_session_data(cls, v):
        """Ensure session_data is a dictionary if provided."""
        try:
            if v is None:
                return {}
            return v
        except Exception as e:
            logger.error(f"Session data validation failed: {e}")
            return {}


class ErrorMessage(BaseMessage):
    """Error message for communication issues."""

    type: MessageType = MessageType.ERROR
    request_id: Optional[str] = Field(
        default=None, description="Related request ID if applicable"
    )
    error_code: ErrorCode = Field(..., description="Specific error code")
    error_message: str = Field(
        ..., min_length=1, description="Human-readable error message"
    )

    @field_validator("error_message")
    @classmethod
    def validate_error_message(cls, v):
        """Validate error message is not empty."""
        try:
            if not v.strip():
                raise ValueError("Error message cannot be empty")
            return v.strip()
        except Exception as e:
            logger.error(f"Error message validation failed: {e}")
            raise


class HeartbeatMessage(BaseMessage):
    """Heartbeat message for connection health monitoring."""

    type: MessageType = MessageType.HEARTBEAT
    request_id: Optional[str] = Field(
        default=None, description="Optional request ID for correlation"
    )
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))


# ====================================================================
# MESSAGE CREATION UTILITIES
# ====================================================================


def create_command_request(
    command: str, parameters: Optional[Dict[str, Any]] = None, timeout_ms: int = 30000
) -> CommandRequest:
    """
    Create a command request message.

    Args:
        command: WinDbg command to execute
        parameters: Optional parameters for the command
        timeout_ms: Timeout in milliseconds

    Returns:
        CommandRequest instance

    Raises:
        ValueError: If command is invalid
    """
    try:
        return CommandRequest(
            command=command, parameters=parameters or {}, timeout_ms=timeout_ms
        )
    except Exception as e:
        logger.error(f"Failed to create command request: {e}")
        raise


def create_response(
    request_id: str,
    success: bool,
    output: str = "",
    error_message: str = "",
    execution_time_ms: int = 0,
    session_data: Optional[Dict[str, Any]] = None,
    command_executed: str = "",
) -> CommandResponse:
    """
    Create a command response message.

    Args:
        request_id: Matching request ID
        success: Whether command execution was successful
        output: Command output
        error_message: Error message if failed
        execution_time_ms: Execution time in milliseconds
        session_data: Current session state
        command_executed: Actual command executed

    Returns:
        CommandResponse instance
    """
    try:
        return CommandResponse(
            request_id=request_id,
            success=success,
            output=output,
            error_message=error_message,
            execution_time_ms=execution_time_ms,
            session_data=session_data or {},
            command_executed=command_executed,
        )
    except Exception as e:
        logger.error(f"Failed to create command response: {e}")
        raise


def create_error(
    request_id: Optional[str], error_code: ErrorCode, error_message: str
) -> ErrorMessage:
    """
    Create an error message.

    Args:
        request_id: Related request ID if applicable
        error_code: Specific error code
        error_message: Human-readable error message

    Returns:
        ErrorMessage instance

    Raises:
        ValueError: If error message is invalid
    """
    try:
        return ErrorMessage(
            request_id=request_id, error_code=error_code, error_message=error_message
        )
    except Exception as e:
        logger.error(f"Failed to create error message: {e}")
        raise


def create_heartbeat(request_id: Optional[str] = None) -> HeartbeatMessage:
    """
    Create a heartbeat message.

    Args:
        request_id: Optional request ID for correlation

    Returns:
        HeartbeatMessage instance
    """
    try:
        return HeartbeatMessage(request_id=request_id)
    except Exception as e:
        logger.error(f"Failed to create heartbeat message: {e}")
        raise


# ====================================================================
# MESSAGE VALIDATION UTILITIES
# ====================================================================


def validate_message(message: BaseMessage) -> bool:
    """
    Validate a message structure.

    Args:
        message: Message to validate

    Returns:
        True if message is valid, False otherwise
    """
    try:
        if not isinstance(message, BaseMessage):
            logger.error(f"Invalid message type: {type(message)}")
            return False

        # Validate based on message type
        if isinstance(message, CommandRequest):
            if not message.command.strip():
                logger.error("Command request has empty command")
                return False
        elif isinstance(message, CommandResponse):
            if not message.request_id:
                logger.error("Command response missing request ID")
                return False
        elif isinstance(message, ErrorMessage):
            if not message.error_message.strip():
                logger.error("Error message has empty error message")
                return False

        return True
    except Exception as e:
        logger.error(f"Message validation failed: {e}")
        return False


def get_message_type(message: BaseMessage) -> Optional[MessageType]:
    """
    Get the message type safely.

    Args:
        message: Message to get type from

    Returns:
        MessageType or None if invalid
    """
    try:
        return message.type
    except Exception as e:
        logger.error(f"Failed to get message type: {e}")
        return None


# ====================================================================
# MESSAGE SERIALIZATION UTILITIES
# ====================================================================


def serialize_message(message: BaseMessage) -> bytes:
    """
    Serialize a message to bytes for transmission.

    Args:
        message: Message to serialize

    Returns:
        Serialized message bytes

    Raises:
        ValueError: If message cannot be serialized
    """
    try:
        import json

        message_json = message.json()
        message_bytes = message_json.encode("utf-8")
        return message_bytes
    except Exception as e:
        logger.error(f"Failed to serialize message: {e}")
        raise ValueError(f"Failed to serialize message: {e}")


def parse_message(data: bytes) -> BaseMessage:
    """
    Parse bytes into a message object.

    Args:
        data: Raw message bytes

    Returns:
        Parsed message object

    Raises:
        ValueError: If data cannot be parsed or is invalid
    """
    try:
        import json

        message_str = data.decode("utf-8")
        parsed = json.loads(message_str)

        # Determine message type and create appropriate object
        message_type = parsed.get("type")
        if not message_type:
            raise ValueError("Missing message type")

        if message_type == MessageType.COMMAND.value:
            return CommandRequest(**parsed)
        elif message_type == MessageType.RESPONSE.value:
            return CommandResponse(**parsed)
        elif message_type == MessageType.ERROR.value:
            return ErrorMessage(**parsed)
        elif message_type == MessageType.HEARTBEAT.value:
            return HeartbeatMessage(**parsed)
        else:
            raise ValueError(f"Unknown message type: {message_type}")

    except json.JSONDecodeError as e:
        raise ValueError(f"Invalid JSON: {e}")
    except Exception as e:
        raise ValueError(f"Failed to parse message: {e}")


def validate_message_size(size: int) -> bool:
    """
    Validate that message size is within limits.

    Args:
        size: Message size in bytes

    Returns:
        True if size is valid, False otherwise
    """
    MAX_MESSAGE_SIZE = 1024 * 1024  # 1MB
    return 0 < size <= MAX_MESSAGE_SIZE
