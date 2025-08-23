"""Tests for dx_visualization tool functionality."""

import pytest
import asyncio
from unittest.mock import Mock, AsyncMock, patch
from src.tools.core_tools import create_dx_visualization
from src.core.command_executor import CommandExecutor, CommandResult


class TestDxVisualization:
    """Test dx_visualization tool functionality."""

    @pytest.fixture
    def mock_command_executor(self):
        """Create a mock command executor."""
        executor = Mock(spec=CommandExecutor)
        executor.execute_command = AsyncMock()
        return executor

    @pytest.fixture
    def dx_tool(self, mock_command_executor):
        """Create the dx_visualization tool."""
        return create_dx_visualization(mock_command_executor)

    @pytest.mark.asyncio
    async def test_basic_dx_command(self, dx_tool, mock_command_executor):
        """Test basic dx command execution."""
        # Setup mock response
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Debugger.Settings\n    Debug\n    Display\n    EngineInitialization",
            error_message=None,
            execution_time_ms=100
        )

        # Execute the tool
        result = await dx_tool({
            "expression": "Debugger.Settings"
        })

        # Verify the command was called correctly
        mock_command_executor.execute_command.assert_called_once_with(
            "dx Debugger.Settings",
            timeout_ms=30000
        )

        # Verify the result
        assert "dx Visualization (Debugger.Settings):" in result
        assert "Debugger.Settings" in result
        assert "Debug" in result

    @pytest.mark.asyncio
    async def test_dx_with_grid_option(self, dx_tool, mock_command_executor):
        """Test dx command with grid option."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Grid view of Debugger.Sessions",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "Debugger.Sessions",
            "options": {"grid": True}
        })

        # Verify the command includes grid flag
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -g Debugger.Sessions",
            timeout_ms=30000
        )

        assert "dx Visualization (Debugger.Sessions):" in result

    @pytest.mark.asyncio
    async def test_dx_with_recursion(self, dx_tool, mock_command_executor):
        """Test dx command with recursion level."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Recursive view with 3 levels",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "@$curprocess.Environment",
            "options": {"recursion_level": 3}
        })

        # Verify the command includes recursion flag
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -r3 @$curprocess.Environment",
            timeout_ms=30000
        )

        assert "dx Visualization (@$curprocess.Environment):" in result

    @pytest.mark.asyncio
    async def test_dx_with_multiple_options(self, dx_tool, mock_command_executor):
        """Test dx command with multiple options."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Verbose grid view",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "Debugger.Sessions",
            "options": {
                "grid": True,
                "verbose": True,
                "recursion_level": 2,
                "format_specifier": "x"
            }
        })

        # Verify the command includes all flags
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -g -v -r2 Debugger.Sessions,x",
            timeout_ms=30000
        )

        assert "dx Visualization (Debugger.Sessions):" in result

    @pytest.mark.asyncio
    async def test_dx_with_native_only(self, dx_tool, mock_command_executor):
        """Test dx command with native only option."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Native structure view",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "nt!PsIdleProcess",
            "options": {"native_only": True}
        })

        # Verify the command includes native flag
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -n nt!PsIdleProcess",
            timeout_ms=30000
        )

        assert "dx Visualization (nt!PsIdleProcess):" in result

    @pytest.mark.asyncio
    async def test_dx_with_custom_timeout(self, dx_tool, mock_command_executor):
        """Test dx command with custom timeout."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Custom timeout test",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "Debugger.Settings",
            "timeout": 60000
        })

        # Verify the command uses custom timeout
        mock_command_executor.execute_command.assert_called_once_with(
            "dx Debugger.Settings",
            timeout_ms=60000
        )

    @pytest.mark.asyncio
    async def test_dx_command_error(self, dx_tool, mock_command_executor):
        """Test dx command error handling."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=False,
            output="Error output",
            error_message="Expression not found",
            execution_time_ms=50
        )

        result = await dx_tool({
            "expression": "InvalidExpression"
        })

        # Verify error is handled properly
        assert "Error executing dx command" in result
        assert "InvalidExpression" in result
        assert "Expression not found" in result

    @pytest.mark.asyncio
    async def test_dx_empty_expression(self, dx_tool):
        """Test dx command with empty expression."""
        result = await dx_tool({
            "expression": ""
        })

        assert "Error: No expression specified" in result

    @pytest.mark.asyncio
    async def test_dx_missing_expression(self, dx_tool):
        """Test dx command with missing expression."""
        result = await dx_tool({})

        assert "Error: No expression specified" in result

    @pytest.mark.asyncio
    async def test_dx_with_grid_cell_size(self, dx_tool, mock_command_executor):
        """Test dx command with grid cell size option."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Grid with cell size limit",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "Debugger.Sessions",
            "options": {"grid_cell_size": 20}
        })

        # Verify the command includes grid cell size flag
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -gc 20 Debugger.Sessions",
            timeout_ms=30000
        )

    @pytest.mark.asyncio
    async def test_dx_with_container_skip(self, dx_tool, mock_command_executor):
        """Test dx command with container skip option."""
        mock_command_executor.execute_command.return_value = CommandResult(
            success=True,
            output="Container with skip",
            error_message=None,
            execution_time_ms=100
        )

        result = await dx_tool({
            "expression": "Debugger.Sessions",
            "options": {"container_skip": 5}
        })

        # Verify the command includes container skip flag
        mock_command_executor.execute_command.assert_called_once_with(
            "dx -c 5 Debugger.Sessions",
            timeout_ms=30000
        )
