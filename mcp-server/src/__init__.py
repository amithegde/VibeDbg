"""
VibeDbg MCP Server Package.

This package provides LLM-driven Windows debugging capabilities through
the Model Context Protocol (MCP).
"""

import logging

logger = logging.getLogger(__name__)

try:
    from .config import config, ServerConfig, OptimizationLevel
    from .core.communication import (
        CommunicationManager,
        CommunicationError,
        TimeoutError,
    )
    from .core.command_executor import CommandExecutor, CommandResult, ExecutionContext
    from .server import VibeDbgMCPServer, main

    __version__ = "0.1.0"
    __author__ = "VibeDbg Team"

    __all__ = [
        "config",
        "ServerConfig",
        "OptimizationLevel",
        "CommunicationManager",
        "CommunicationError",
        "TimeoutError",
        "CommandExecutor",
        "CommandResult",
        "ExecutionContext",
        "VibeDbgMCPServer",
        "main",
    ]

    logger.debug("VibeDbg MCP Server package imported successfully")

except ImportError as e:
    logger.error(f"Failed to import VibeDbg MCP Server components: {e}")
    # Provide fallback values for critical imports
    __version__ = "0.1.0"
    __author__ = "VibeDbg Team"
    __all__ = []
    raise
