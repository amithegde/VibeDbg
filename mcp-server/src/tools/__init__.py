"""
VibeDbg MCP Server Tools.

This module provides streamlined core tools optimized for LLM-driven
Windows debugging capabilities. The tools are designed to be simple,
composable, and easily discoverable by LLMs.
"""

import logging

logger = logging.getLogger(__name__)

try:
    from .core_tools import get_core_tools

    __all__ = [
        "get_core_tools",
    ]

    logger.debug("VibeDbg MCP Server tools imported successfully")

except ImportError as e:
    logger.error(f"Failed to import VibeDbg MCP Server tools: {e}")
    __all__ = []
    raise
