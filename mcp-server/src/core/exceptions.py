"""
Centralized exception handling for VibeDbg MCP Server.

This module provides a comprehensive exception hierarchy and utilities for
robust error handling throughout the application.
"""

import logging
import traceback
from typing import Optional, Dict, Any, Union
from enum import Enum

logger = logging.getLogger(__name__)


class ErrorSeverity(Enum):
    """Error severity levels."""

    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"
    CRITICAL = "critical"


class ErrorCategory(Enum):
    """Error categories for classification."""

    COMMUNICATION = "communication"
    VALIDATION = "validation"
    CONFIGURATION = "configuration"
    EXECUTION = "execution"
    TIMEOUT = "timeout"
    RESOURCE = "resource"
    SECURITY = "security"
    UNKNOWN = "unknown"


class VibeDbgError(Exception):
    """Base exception class for VibeDbg-specific errors."""

    def __init__(
        self,
        message: str,
        category: ErrorCategory = ErrorCategory.UNKNOWN,
        severity: ErrorSeverity = ErrorSeverity.MEDIUM,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(message)
        self.message = message
        self.category = category
        self.severity = severity
        self.details = details or {}
        self.cause = cause

        # Log the error
        self._log_error()

    def _log_error(self):
        """Log the error with appropriate level based on severity."""
        log_message = f"[{self.category.value.upper()}] {self.message}"

        if self.details:
            log_message += f" | Details: {self.details}"

        if self.severity == ErrorSeverity.CRITICAL:
            logger.critical(log_message, exc_info=self.cause)
        elif self.severity == ErrorSeverity.HIGH:
            logger.error(log_message, exc_info=self.cause)
        elif self.severity == ErrorSeverity.MEDIUM:
            logger.warning(log_message)
        else:
            logger.info(log_message)

    def to_dict(self) -> Dict[str, Any]:
        """Convert error to dictionary for serialization."""
        return {
            "message": self.message,
            "category": self.category.value,
            "severity": self.severity.value,
            "details": self.details,
            "type": self.__class__.__name__,
        }


class CommunicationError(VibeDbgError):
    """Raised when communication with WinDbg extension fails."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.COMMUNICATION,
            severity=ErrorSeverity.HIGH,
            details=details,
            cause=cause,
        )


class ValidationError(VibeDbgError):
    """Raised when input validation fails."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.VALIDATION,
            severity=ErrorSeverity.MEDIUM,
            details=details,
            cause=cause,
        )


class ConfigurationError(VibeDbgError):
    """Raised when configuration is invalid or missing."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.CONFIGURATION,
            severity=ErrorSeverity.HIGH,
            details=details,
            cause=cause,
        )


class ExecutionError(VibeDbgError):
    """Raised when command execution fails."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.EXECUTION,
            severity=ErrorSeverity.MEDIUM,
            details=details,
            cause=cause,
        )


class TimeoutError(VibeDbgError):
    """Raised when operations timeout."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.TIMEOUT,
            severity=ErrorSeverity.MEDIUM,
            details=details,
            cause=cause,
        )


class ResourceError(VibeDbgError):
    """Raised when resource allocation or management fails."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.RESOURCE,
            severity=ErrorSeverity.HIGH,
            details=details,
            cause=cause,
        )


class SecurityError(VibeDbgError):
    """Raised when security violations are detected."""

    def __init__(
        self,
        message: str,
        details: Optional[Dict[str, Any]] = None,
        cause: Optional[Exception] = None,
    ):
        super().__init__(
            message=message,
            category=ErrorCategory.SECURITY,
            severity=ErrorSeverity.CRITICAL,
            details=details,
            cause=cause,
        )


class ErrorHandler:
    """Centralized error handler for consistent error processing."""

    @staticmethod
    def handle_exception(
        exception: Exception,
        context: str = "",
        additional_details: Optional[Dict[str, Any]] = None,
    ) -> VibeDbgError:
        """
        Handle any exception and convert to appropriate VibeDbgError.

        Args:
            exception: The original exception
            context: Context where the error occurred
            additional_details: Additional error details

        Returns:
            Appropriate VibeDbgError subclass
        """
        details = additional_details or {}
        if context:
            details["context"] = context

        # Preserve original exception information
        details["original_exception"] = str(exception)
        details["exception_type"] = type(exception).__name__
        details["traceback"] = traceback.format_exc()

        # Map common exceptions to VibeDbgError types
        if isinstance(exception, (ConnectionError, OSError)):
            return CommunicationError(
                message=f"Communication error: {str(exception)}",
                details=details,
                cause=exception,
            )
        elif isinstance(exception, (ValueError, TypeError)):
            return ValidationError(
                message=f"Validation error: {str(exception)}",
                details=details,
                cause=exception,
            )
        elif isinstance(exception, FileNotFoundError):
            return ConfigurationError(
                message=f"Configuration file not found: {str(exception)}",
                details=details,
                cause=exception,
            )
        elif isinstance(exception, PermissionError):
            return SecurityError(
                message=f"Permission denied: {str(exception)}",
                details=details,
                cause=exception,
            )
        elif isinstance(exception, (MemoryError, ResourceWarning)):
            return ResourceError(
                message=f"Resource error: {str(exception)}",
                details=details,
                cause=exception,
            )
        elif isinstance(exception, TimeoutError):
            return TimeoutError(
                message=f"Operation timed out: {str(exception)}",
                details=details,
                cause=exception,
            )
        else:
            # Generic error for unknown exceptions
            return ExecutionError(
                message=f"Unexpected error: {str(exception)}",
                details=details,
                cause=exception,
            )

    @staticmethod
    def safe_execute(
        func, *args, context: str = "", **kwargs
    ) -> Union[Any, VibeDbgError]:
        """
        Safely execute a function with proper error handling.

        Args:
            func: Function to execute
            *args: Function arguments
            context: Context description
            **kwargs: Function keyword arguments

        Returns:
            Function result or VibeDbgError on failure
        """
        try:
            return func(*args, **kwargs)
        except Exception as e:
            return ErrorHandler.handle_exception(e, context=context)

    @staticmethod
    async def safe_execute_async(
        func, *args, context: str = "", **kwargs
    ) -> Union[Any, VibeDbgError]:
        """
        Safely execute an async function with proper error handling.

        Args:
            func: Async function to execute
            *args: Function arguments
            context: Context description
            **kwargs: Function keyword arguments

        Returns:
            Function result or VibeDbgError on failure
        """
        try:
            return await func(*args, **kwargs)
        except Exception as e:
            return ErrorHandler.handle_exception(e, context=context)

    @staticmethod
    def log_error_summary(errors: list[VibeDbgError]):
        """
        Log a summary of multiple errors.

        Args:
            errors: List of VibeDbgError instances
        """
        if not errors:
            return

        error_counts = {}
        critical_errors = []

        for error in errors:
            category = error.category.value
            error_counts[category] = error_counts.get(category, 0) + 1

            if error.severity == ErrorSeverity.CRITICAL:
                critical_errors.append(error)

        summary = f"Error summary: {len(errors)} total errors"
        for category, count in error_counts.items():
            summary += f", {count} {category}"

        if critical_errors:
            logger.critical(
                f"{summary}. {len(critical_errors)} critical errors require immediate attention."
            )
            for error in critical_errors:
                logger.critical(f"CRITICAL: {error.message}")
        else:
            logger.warning(summary)


def with_error_handling(context: str = ""):
    """
    Decorator for automatic error handling.

    Args:
        context: Context description for errors
    """

    def decorator(func):
        if hasattr(func, "__await__"):
            # Async function
            async def async_wrapper(*args, **kwargs):
                try:
                    return await func(*args, **kwargs)
                except Exception as e:
                    error = ErrorHandler.handle_exception(
                        e, context=context or func.__name__
                    )
                    raise error

            return async_wrapper
        else:
            # Sync function
            def sync_wrapper(*args, **kwargs):
                try:
                    return func(*args, **kwargs)
                except Exception as e:
                    error = ErrorHandler.handle_exception(
                        e, context=context or func.__name__
                    )
                    raise error

            return sync_wrapper

    return decorator


# Convenience functions for common error patterns
def ensure_not_none(value: Any, name: str = "value") -> Any:
    """Ensure a value is not None, raise ValidationError if it is."""
    if value is None:
        raise ValidationError(f"{name} cannot be None")
    return value


def ensure_type(value: Any, expected_type: type, name: str = "value") -> Any:
    """Ensure a value is of expected type, raise ValidationError if not."""
    if not isinstance(value, expected_type):
        raise ValidationError(
            f"{name} must be of type {expected_type.__name__}, got {type(value).__name__}"
        )
    return value


def ensure_in_range(
    value: Union[int, float],
    min_val: Union[int, float],
    max_val: Union[int, float],
    name: str = "value",
) -> Union[int, float]:
    """Ensure a numeric value is within range, raise ValidationError if not."""
    if not (min_val <= value <= max_val):
        raise ValidationError(
            f"{name} must be between {min_val} and {max_val}, got {value}"
        )
    return value


def ensure_not_empty(value: str, name: str = "value") -> str:
    """Ensure a string is not empty, raise ValidationError if it is."""
    if not value or not value.strip():
        raise ValidationError(f"{name} cannot be empty")
    return value.strip()
