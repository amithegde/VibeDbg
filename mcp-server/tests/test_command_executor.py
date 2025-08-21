"""Tests for command executor functionality."""

import pytest
import asyncio
from unittest.mock import Mock, AsyncMock, patch
from src.core.command_executor import (
    CommandExecutor,
    ExecutionContext,
    CommandResult,
    CommandRouter,
    CommandValidator,
    CommandCache,
)


class TestExecutionContext:
    """Test ExecutionContext functionality."""

    def test_context_creation(self):
        """Test context creation."""
        context = ExecutionContext(session_id="test_session")

        assert context.session_id == "test_session"
        assert context.current_process is None
        assert context.current_thread is None
        # breakpoints defaults to empty list, not None
        assert context.breakpoints == []
        assert context.last_command is None

    def test_context_with_values(self):
        """Test context creation with values."""
        breakpoints = [{"address": "0x1234", "enabled": True}]
        context = ExecutionContext(
            session_id="test",
            current_process="notepad.exe",
            current_thread=1234,
            breakpoints=breakpoints,
        )

        assert context.current_process == "notepad.exe"
        assert context.current_thread == 1234
        assert context.breakpoints == breakpoints


class TestCommandValidator:
    """Test CommandValidator functionality."""

    @pytest.fixture
    def validator(self):
        return CommandValidator()

    def test_validate_safe_command(self, validator):
        """Test validation of safe commands."""
        assert validator.is_safe_command("k") is True
        assert validator.is_safe_command("r") is True
        assert validator.is_safe_command("lm") is True
        assert validator.is_safe_command("~") is True

    def test_validate_dangerous_command(self, validator):
        """Test validation of dangerous commands."""
        assert validator.is_safe_command("format c:") is False
        assert validator.is_safe_command("delete /f /q *") is False
        assert validator.is_safe_command("shutdown -r -t 0") is False

    def test_validate_command_syntax(self, validator):
        """Test command syntax validation."""
        # Safe commands should pass
        assert validator.validate_command_syntax("k") is True
        assert validator.validate_command_syntax("r") is True

        # Empty commands should fail
        assert validator.validate_command_syntax("") is False
        assert validator.validate_command_syntax("   ") is False


class TestCommandCache:
    """Test CommandCache functionality."""

    @pytest.fixture
    def cache(self):
        return CommandCache()

    @pytest.fixture
    def test_context(self):
        return ExecutionContext(session_id="test")

    @pytest.mark.asyncio
    async def test_cache_operations(self, cache, test_context):
        """Test cache operations."""
        result = CommandResult(
            success=True, output="test output", execution_time_ms=100
        )

        # Cache the result
        await cache.cache_result("test_command", result, test_context)

        # Retrieve cached result
        cached_result = await cache.get_cached_result("test_command", test_context)

        assert cached_result is not None
        assert cached_result.output == "test output"
        assert cached_result.success is True
        assert cached_result.cached is True

    @pytest.mark.asyncio
    async def test_cache_miss(self, cache, test_context):
        """Test cache miss."""
        result = await cache.get_cached_result("nonexistent_command", test_context)
        assert result is None


class TestCommandExecutor:
    """Test CommandExecutor functionality."""

    @pytest.fixture
    def executor(self):
        """Create a CommandExecutor instance."""
        executor = CommandExecutor()
        # Mock the communication manager to avoid actual WinDbg communication
        executor.comm_manager = Mock()
        executor.comm_manager.send_command = AsyncMock()
        return executor

    def test_executor_initialization(self, executor):
        """Test executor initializes correctly."""
        assert executor.comm_manager is not None
        assert executor.router is not None
        assert executor.cache is not None
        assert executor.validator is not None
        assert executor.context is not None

    @pytest.mark.asyncio
    async def test_execute_invalid_command(self, executor):
        """Test execution of invalid command."""
        # Empty command should fail validation
        result = await executor.execute_command("")
        assert result.success is False
        assert "invalid" in result.error_message.lower()

    @pytest.mark.asyncio
    async def test_execute_whitespace_command(self, executor):
        """Test execution of whitespace-only command."""
        # Whitespace command should fail validation
        result = await executor.execute_command("   ")
        assert result.success is False
        assert "invalid" in result.error_message.lower()

    @pytest.mark.asyncio
    async def test_execute_sequence(self, executor):
        """Test executing a sequence of commands."""
        with patch.object(executor, "execute_command") as mock_execute:
            mock_execute.return_value = CommandResult(
                success=True, output="Command output", execution_time_ms=50
            )

            results = await executor.execute_sequence(["k", "r", "lm"])

            assert len(results) == 3
            for result in results:
                assert result.success is True

            assert mock_execute.call_count == 3
