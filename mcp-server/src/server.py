"""
VibeDbg MCP Server implementation using the official MCP SDK.

This module provides the main server implementation for LLM-driven Windows debugging
capabilities through the Model Context Protocol (MCP).
"""

import asyncio
import json
import logging
import sys
import time
from typing import Any, Dict, List, Optional

from mcp.server import Server
from mcp.server.models import InitializationOptions
from mcp.server.stdio import stdio_server
from mcp.types import (
    CallToolRequest,
    CallToolResult,
    ListToolsRequest,
    ListToolsResult,
    Tool,
    TextContent,
)

from .config import ServerConfig
from .core.command_executor import CommandExecutor, CommandResult, ExecutionContext
from .tools.core_tools import get_core_tools
from .tools.tool_metadata import get_tool_metadata

logger = logging.getLogger(__name__)


class VibeDbgMCPServer:
    """VibeDbg MCP Server for Windows debugging capabilities."""

    def __init__(self, config: Optional[ServerConfig] = None, debug: bool = False):
        """Initialize the VibeDbg MCP Server.

        Args:
            config: Server configuration. If None, uses default configuration.
            debug: Enable debug mode with enhanced logging.
        """
        try:
            self.config = config or ServerConfig()
            self.debug = debug
            self.command_executor = CommandExecutor()
            self.server = Server("vibedbg-mcp-server")

            # Tools will be registered asynchronously
            self.tools = {}

            # Setup debug logging if enabled
            if self.debug:
                self._setup_debug_logging()

            # Register server capabilities and handlers
            self._register_capabilities()
            logger.info("VibeDbg MCP Server initialized successfully")
        except ImportError as e:
            logger.error(f"Missing required dependencies: {e}")
            raise RuntimeError(f"Failed to initialize due to missing dependencies: {e}")
        except AttributeError as e:
            logger.error(f"Invalid configuration or dependency: {e}")
            raise ValueError(f"Invalid configuration: {e}")
        except Exception as e:
            logger.error(
                f"Unexpected error initializing VibeDbg MCP Server: {e}", exc_info=True
            )
            raise RuntimeError(f"Initialization failed: {e}")

    def _setup_debug_logging(self):
        """Setup enhanced debug logging."""
        try:
            debug_logger = logging.getLogger()
            debug_logger.setLevel(logging.DEBUG)

            # Add file handler for debug logs
            try:
                file_handler = logging.FileHandler("vibedbg_mcp_debug.log")
                file_handler.setLevel(logging.DEBUG)
            except (OSError, IOError) as e:
                logger.warning(f"Could not create debug log file: {e}")
                file_handler = None

            # Add stderr handler
            try:
                stderr_handler = logging.StreamHandler(sys.stderr)
                stderr_handler.setLevel(logging.DEBUG)
            except Exception as e:
                logger.warning(f"Could not create stderr handler: {e}")
                stderr_handler = None

            # Set formatter
            try:
                formatter = logging.Formatter(
                    "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
                )
                if file_handler:
                    file_handler.setFormatter(formatter)
                if stderr_handler:
                    stderr_handler.setFormatter(formatter)
            except Exception as e:
                logger.warning(f"Could not set log formatter: {e}")

            # Add handlers if not already present
            try:
                if file_handler and not any(
                    isinstance(h, logging.FileHandler) for h in debug_logger.handlers
                ):
                    debug_logger.addHandler(file_handler)
                if stderr_handler and not any(
                    isinstance(h, logging.StreamHandler) and h.stream == sys.stderr
                    for h in debug_logger.handlers
                ):
                    debug_logger.addHandler(stderr_handler)
            except Exception as e:
                logger.warning(f"Could not add log handlers: {e}")
        except Exception as e:
            logger.error(f"Failed to setup debug logging: {e}", exc_info=True)

    async def initialize(self):
        """Initialize the server asynchronously."""
        try:
            # Initialize command executor first
            await self.command_executor.initialize()

            # Register tools
            await self._register_tools()
            logger.info("Server initialization completed successfully")
        except ImportError as e:
            logger.error(f"Missing required dependencies during initialization: {e}")
            raise RuntimeError(
                f"Initialization failed due to missing dependencies: {e}"
            )
        except AttributeError as e:
            logger.error(f"Invalid configuration during initialization: {e}")
            raise ValueError(f"Invalid configuration during initialization: {e}")
        except Exception as e:
            logger.error(
                f"Unexpected error during server initialization: {e}", exc_info=True
            )
            raise RuntimeError(f"Server initialization failed: {e}")

    def _register_capabilities(self):
        """Register server capabilities."""
        try:
            # Register tools capability
            self.server.request_handlers[ListToolsRequest] = self._handle_list_tools
            self.server.request_handlers[CallToolRequest] = self._handle_call_tool
            logger.debug("Server capabilities registered successfully")
        except AttributeError as e:
            logger.error(f"Invalid server object during capability registration: {e}")
            raise ValueError(f"Invalid server configuration: {e}")
        except Exception as e:
            logger.error(
                f"Unexpected error registering server capabilities: {e}", exc_info=True
            )
            raise RuntimeError(f"Failed to register server capabilities: {e}")

    async def _register_tools(self):
        """Register core debugging tools optimized for LLM interaction."""
        try:
            self.tools = {}

            # Register streamlined core tools
            core_tools = get_core_tools(self.command_executor)
            for name, tool_func in core_tools.items():
                if not callable(tool_func):
                    logger.warning(f"Tool '{name}' is not callable, skipping")
                    continue
                self.tools[name] = tool_func

            if not self.tools:
                raise RuntimeError("No valid tools were registered")

            logger.info(
                f"Registered {len(self.tools)} core tools: {list(self.tools.keys())}"
            )
        except ValueError as e:
            logger.error(f"Invalid tool configuration: {e}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error registering tools: {e}", exc_info=True)
            raise RuntimeError(f"Failed to register tools: {e}")

    async def _handle_list_tools(self, params: ListToolsRequest) -> ListToolsResult:
        """Handle list tools request.

        Args:
            params: List tools request parameters.

        Returns:
            List of available tools.
        """
        try:
            tools = []
            for tool_name, tool_func in self.tools.items():
                try:
                    # Get tool metadata from the centralized metadata store
                    tool_metadata = get_tool_metadata(tool_name)

                    tool = Tool(
                        name=tool_name,
                        description=tool_metadata.get(
                            "description", f"Execute {tool_name}"
                        ),
                        inputSchema=tool_metadata.get("input_schema", {}),
                    )
                    tools.append(tool)
                except Exception as e:
                    logger.warning(
                        f"Failed to create tool metadata for '{tool_name}': {e}"
                    )
                    # Continue with other tools
                    continue

            logger.debug(f"Listed {len(tools)} tools successfully")
            return ListToolsResult(tools=tools)
        except Exception as e:
            logger.error(f"Unexpected error listing tools: {e}", exc_info=True)
            raise RuntimeError(f"Failed to list tools: {e}")

    async def _handle_call_tool(self, params: CallToolRequest) -> CallToolResult:
        """Handle tool call request with comprehensive logging.

        Args:
            params: Tool call request parameters.

        Returns:
            Tool execution result.
        """
        tool_name = params.params.name
        arguments = params.params.arguments or {}

        logger.info(
            f"[MCP_SERVER] Received tool call: '{tool_name}' with args: {arguments}"
        )

        if tool_name not in self.tools:
            error_msg = (
                f"Unknown tool: {tool_name}. Available tools: {list(self.tools.keys())}"
            )
            logger.error(f"[MCP_SERVER] {error_msg}")
            return CallToolResult(
                content=[TextContent(type="text", text=f"Error: {error_msg}")],
                isError=True,
            )

        start_time = time.time()
        try:
            # Execute the tool
            tool_func = self.tools[tool_name]
            logger.debug(f"[MCP_SERVER] Executing tool function for: '{tool_name}'")

            if not callable(tool_func):
                raise ValueError(f"Tool '{tool_name}' is not callable")

            result = await tool_func(arguments)

            # Convert result to MCP format
            content = []
            if isinstance(result, str):
                content.append(TextContent(type="text", text=result))
            elif isinstance(result, dict):
                # Convert dict to formatted text
                try:
                    formatted_result = json.dumps(result, indent=2)
                    content.append(TextContent(type="text", text=formatted_result))
                except (TypeError, ValueError) as e:
                    logger.warning(
                        f"Failed to serialize dict result for '{tool_name}': {e}"
                    )
                    content.append(TextContent(type="text", text=str(result)))
            else:
                content.append(TextContent(type="text", text=str(result)))

            execution_time = int((time.time() - start_time) * 1000)
            logger.info(
                f"[MCP_SERVER] Tool '{tool_name}' completed successfully in {execution_time}ms"
            )
            logger.debug(
                f"[MCP_SERVER] Tool '{tool_name}' result preview: {str(result)[:500]}..."
            )

            return CallToolResult(
                content=content,
                isError=False,
            )

        except ValueError as e:
            execution_time = int((time.time() - start_time) * 1000)
            error_msg = f"Tool '{tool_name}' failed with invalid parameters after {execution_time}ms: {str(e)}"
            logger.error(f"[MCP_SERVER] {error_msg}")
            return CallToolResult(
                content=[
                    TextContent(
                        type="text", text=f"Error: Invalid parameters - {str(e)}"
                    )
                ],
                isError=True,
            )
        except Exception as e:
            execution_time = int((time.time() - start_time) * 1000)
            error_msg = f"Tool '{tool_name}' failed with unexpected error after {execution_time}ms: {str(e)}"
            logger.error(f"[MCP_SERVER] {error_msg}", exc_info=True)
            return CallToolResult(
                content=[
                    TextContent(type="text", text=f"Error: Unexpected error - {str(e)}")
                ],
                isError=True,
            )


async def main(debug: bool = False):
    """Main entry point for the VibeDbg MCP Server.

    Args:
        debug: Enable debug mode with enhanced logging.
    """
    server = None
    try:
        # Configure logging
        log_level = logging.DEBUG if debug else logging.INFO
        logging.basicConfig(
            level=log_level,
            format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        )

        # Create server
        server = VibeDbgMCPServer(debug=debug)

        # Initialize the server
        await server.initialize()

        # Run the server using stdio
        async with stdio_server() as (read_stream, write_stream):
            await server.server.run(
                read_stream,
                write_stream,
                InitializationOptions(
                    protocolVersion="2024-11-05",
                    server_name="vibedbg-mcp-server",
                    server_version="0.1.0",
                    capabilities={"tools": {}},
                    clientInfo={
                        "name": "vibedbg-client",
                        "version": "0.1.0",
                    },
                    workspaceFolders=[],
                ),
            )
    except KeyboardInterrupt:
        logger.info("Server shutdown requested by user")
    except ImportError as e:
        logger.error(f"Missing required dependencies: {e}")
        raise RuntimeError(f"Server startup failed due to missing dependencies: {e}")
    except Exception as e:
        logger.error(f"Server startup failed with unexpected error: {e}", exc_info=True)
        raise RuntimeError(f"Server startup failed: {e}")
    finally:
        # Cleanup server resources
        if server and hasattr(server, "command_executor"):
            try:
                await server.command_executor.shutdown()
            except Exception as e:
                logger.error(f"Error during server cleanup: {e}", exc_info=True)


async def main_debug():
    """Debug entry point for the VibeDbg MCP Server."""
    try:
        await main(debug=True)
    except Exception as e:
        logger.error(f"Debug server failed: {e}", exc_info=True)
        raise


if __name__ == "__main__":
    asyncio.run(main())
