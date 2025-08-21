"""Tests for MCP server functionality."""

import pytest
import asyncio
from unittest.mock import Mock, AsyncMock
from src.server import VibeDbgMCPServer
from src.config import ServerConfig
from mcp.types import CallToolRequest, ListToolsRequest


class TestVibeDbgMCPServer:
    """Test VibeDbg MCP Server functionality."""

    @pytest.fixture
    def server(self):
        """Create a test server instance."""
        config = ServerConfig()
        server = VibeDbgMCPServer(config)
        return server

    def test_server_initialization(self, server):
        """Test server initializes correctly."""
        assert server.config is not None
        assert server.command_executor is not None
        assert server.server is not None
        assert isinstance(server.tools, dict)

    @pytest.mark.asyncio
    async def test_initialize(self, server):
        """Test server initialization."""
        # Mock the command executor to avoid actual WinDbg communication
        server.command_executor.initialize = AsyncMock()

        await server.initialize()

        # After initialization, tools should be registered
        assert len(server.tools) > 0

    @pytest.mark.asyncio
    async def test_list_tools(self, server):
        """Test tool listing functionality."""
        # Initialize server with mock tools
        server.tools = {"test_tool": AsyncMock(), "another_tool": AsyncMock()}

        # Create mock request
        request = Mock()
        request.params = Mock()

        result = await server._handle_list_tools(request)

        assert len(result.tools) == 2
        tool_names = [tool.name for tool in result.tools]
        assert "test_tool" in tool_names
        assert "another_tool" in tool_names

    @pytest.mark.asyncio
    async def test_call_tool_success(self, server):
        """Test successful tool execution."""
        # Mock tool function
        mock_tool = AsyncMock(return_value="Test result")
        server.tools["test_tool"] = mock_tool

        # Create mock request
        request = Mock()
        request.params = Mock()
        request.params.name = "test_tool"
        request.params.arguments = {"arg1": "value1"}

        result = await server._handle_call_tool(request)

        assert result.isError is False
        assert len(result.content) == 1
        assert result.content[0].text == "Test result"

        # Verify tool was called with correct arguments
        mock_tool.assert_called_once_with({"arg1": "value1"})

    @pytest.mark.asyncio
    async def test_call_tool_unknown(self, server):
        """Test calling unknown tool."""
        request = Mock()
        request.params = Mock()
        request.params.name = "unknown_tool"
        request.params.arguments = {}

        result = await server._handle_call_tool(request)
        assert result.isError == True
        assert "Unknown tool: unknown_tool" in result.content[0].text

    @pytest.mark.asyncio
    async def test_call_tool_error(self, server):
        """Test tool execution error handling."""
        # Mock tool function that raises exception
        mock_tool = AsyncMock(side_effect=Exception("Test error"))
        server.tools["failing_tool"] = mock_tool

        request = Mock()
        request.params = Mock()
        request.params.name = "failing_tool"
        request.params.arguments = {}

        result = await server._handle_call_tool(request)

        assert result.isError is True
        assert "Error: Unexpected error - Test error" in result.content[0].text
