"""
VibeDbg MCP Server submodule.
"""

import logging

logger = logging.getLogger(__name__)

try:
    from ..server import VibeDbgMCPServer, main

    __all__ = ["VibeDbgMCPServer", "main"]

    logger.debug("VibeDbg MCP Server submodule imported successfully")

except ImportError as e:
    logger.error(f"Failed to import VibeDbg MCP Server submodule: {e}")
    __all__ = []
    raise
