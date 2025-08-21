"""
VibeDbg MCP Server entry point.

This module provides the main server implementation for LLM-driven Windows debugging
capabilities through the Model Context Protocol (MCP).
"""

import logging

logger = logging.getLogger(__name__)

try:
    from ...server import VibeDbgMCPServer, main, main_debug

    __all__ = ["VibeDbgMCPServer", "main", "main_debug"]

    logger.debug("VibeDbg MCP Server entry point imported successfully")

except ImportError as e:
    logger.error(f"Failed to import VibeDbg MCP Server entry point: {e}")
    __all__ = []
    raise
