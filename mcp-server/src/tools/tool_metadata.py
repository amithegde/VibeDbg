"""
Tool metadata management for VibeDbg MCP Server.

This module provides centralized metadata for all tools including descriptions,
input schemas, and usage examples to ensure consistent tool documentation.
"""

import logging
from typing import Dict, Any, Optional, List

logger = logging.getLogger(__name__)

# ====================================================================
# TOOL METADATA STORE
# ====================================================================

# Core tool metadata
CORE_TOOL_METADATA = {
    "execute_command": {
        "description": "Execute any WinDbg command. This is the primary tool for running specific debugging commands like memory examination (da, db, dq), register inspection (r), disassembly (u), symbol operations (x, ln), module listing (lm), and process/thread info (|, ~).",
        "input_schema": {
            "type": "object",
            "properties": {
                "command": {
                    "type": "string",
                    "description": "The WinDbg command to execute",
                },
                "timeout": {
                    "type": "integer",
                    "description": "Timeout in milliseconds (default: 30000)",
                    "default": 30000,
                },
            },
            "required": ["command"],
        },
        "examples": [
            {"command": "r", "description": "Display all registers"},
            {"command": "k", "description": "Display stack trace"},
            {"command": "lm", "description": "List loaded modules"},
            {
                "command": "da @esp L10",
                "description": "Display ASCII memory at ESP for 16 bytes",
            },
            {
                "command": "u @rip L5",
                "description": "Disassemble 5 instructions from current RIP",
            },
        ],
    },
    "execute_sequence": {
        "description": "Execute multiple WinDbg commands in sequence. Perfect for complex debugging workflows like examining function parameters then stepping through, setting breakpoints and analyzing crashes, or multiple memory dumps for comparison.",
        "input_schema": {
            "type": "object",
            "properties": {
                "commands": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "List of WinDbg commands to execute in order",
                },
                "stop_on_error": {
                    "type": "boolean",
                    "description": "Whether to stop execution if a command fails (default: false)",
                    "default": False,
                },
                "timeout_per_command": {
                    "type": "integer",
                    "description": "Timeout per command in milliseconds (default: 30000)",
                    "default": 30000,
                },
            },
            "required": ["commands"],
        },
        "examples": [
            {
                "commands": ["r", "k", "dv"],
                "description": "Get registers, stack trace, and local variables",
            },
            {
                "commands": ["bp nt!NtCreateFile", "g", "k"],
                "description": "Set breakpoint, continue, then show stack",
            },
        ],
    },
    "set_breakpoint": {
        "description": "Set a breakpoint at a specific location. Supports addresses, symbol names, and expressions with optional conditions and commands.",
        "input_schema": {
            "type": "object",
            "properties": {
                "location": {
                    "type": "string",
                    "description": "Address, symbol, or expression where to set breakpoint",
                },
                "condition": {
                    "type": "string",
                    "description": "Optional condition for conditional breakpoint",
                },
                "command": {
                    "type": "string",
                    "description": "Optional command to execute when breakpoint hits",
                },
            },
            "required": ["location"],
        },
        "examples": [
            {"location": "nt!NtCreateFile", "description": "Break on file creation"},
            {"location": "0x401000", "description": "Break at specific address"},
            {
                "location": "main+0x10",
                "description": "Break 16 bytes into main function",
            },
            {
                "location": "nt!NtCreateFile",
                "condition": "eax==0",
                "description": "Break only on successful file creation",
            },
        ],
    },
    "step_and_analyze": {
        "description": "Step through code and analyze the current state. This tool combines stepping with analysis to provide context about what happened during the step.",
        "input_schema": {
            "type": "object",
            "properties": {
                "step_type": {
                    "type": "string",
                    "enum": ["over", "into", "out"],
                    "description": "Type of step to perform (default: over)",
                    "default": "over",
                },
                "analysis_depth": {
                    "type": "string",
                    "enum": ["basic", "detailed"],
                    "description": "Depth of analysis after step (default: basic)",
                    "default": "basic",
                },
            },
        },
        "examples": [
            {"step_type": "over", "description": "Step over current instruction"},
            {"step_type": "into", "description": "Step into function call"},
            {"step_type": "out", "description": "Step out of current function"},
            {
                "step_type": "over",
                "analysis_depth": "detailed",
                "description": "Step over with detailed analysis",
            },
        ],
    },
    "analyze_context": {
        "description": "Analyze the current debugging context. Provides a comprehensive view of the current state including process, thread, stack, registers, and memory.",
        "input_schema": {
            "type": "object",
            "properties": {
                "type": {
                    "type": "string",
                    "enum": ["full", "stack", "memory", "process"],
                    "description": "Type of analysis to perform (default: full)",
                    "default": "full",
                }
            },
        },
        "examples": [
            {"type": "full", "description": "Complete context analysis"},
            {"type": "stack", "description": "Stack trace analysis only"},
            {"type": "memory", "description": "Memory and register analysis"},
            {"type": "process", "description": "Process and thread analysis"},
        ],
    },
    "get_session_status": {
        "description": "Get the current debugging session status. Returns information about the current debugging session including process status, breakpoints, and session health.",
        "input_schema": {"type": "object", "properties": {}},
        "examples": [{"description": "Get current session status"}],
    },
}

# ====================================================================
# METADATA MANAGEMENT FUNCTIONS
# ====================================================================


def get_tool_metadata(tool_name: str) -> Dict[str, Any]:
    """
    Get metadata for a specific tool.

    Args:
        tool_name: Name of the tool to get metadata for

    Returns:
        Dictionary containing tool metadata
    """
    try:
        # Check core tools first
        if tool_name in CORE_TOOL_METADATA:
            return CORE_TOOL_METADATA[tool_name]

        # Check other tool categories
        for category_metadata in [CORE_TOOL_METADATA]:
            if tool_name in category_metadata:
                return category_metadata[tool_name]

        # Return default metadata for unknown tools
        logger.warning(f"No metadata found for tool: {tool_name}")
        return {
            "description": f"Execute {tool_name} command",
            "input_schema": {
                "type": "object",
                "properties": {
                    "command": {"type": "string", "description": "Command parameters"}
                },
            },
        }
    except Exception as e:
        logger.error(f"Error getting metadata for tool {tool_name}: {e}")
        return {"description": f"Execute {tool_name}", "input_schema": {}}


def get_all_tool_metadata() -> Dict[str, Dict[str, Any]]:
    """
    Get metadata for all available tools.

    Returns:
        Dictionary mapping tool names to their metadata
    """
    try:
        all_metadata = {}

        # Add core tools
        all_metadata.update(CORE_TOOL_METADATA)

        # Add other categories as needed
        # all_metadata.update(DEBUGGING_TOOL_METADATA)
        # all_metadata.update(ANALYSIS_TOOL_METADATA)
        # etc.

        return all_metadata
    except Exception as e:
        logger.error(f"Error getting all tool metadata: {e}")
        return {}


def validate_tool_metadata(tool_name: str, metadata: Dict[str, Any]) -> bool:
    """
    Validate tool metadata structure.

    Args:
        tool_name: Name of the tool
        metadata: Metadata to validate

    Returns:
        True if metadata is valid, False otherwise
    """
    try:
        required_fields = ["description", "input_schema"]

        for field in required_fields:
            if field not in metadata:
                logger.error(
                    f"Missing required field '{field}' in metadata for tool '{tool_name}'"
                )
                return False

        # Validate input schema structure
        input_schema = metadata.get("input_schema", {})
        if not isinstance(input_schema, dict):
            logger.error(
                f"Invalid input_schema type for tool '{tool_name}': expected dict, got {type(input_schema)}"
            )
            return False

        return True
    except Exception as e:
        logger.error(f"Error validating metadata for tool {tool_name}: {e}")
        return False


def get_tool_examples(tool_name: str) -> List[Dict[str, str]]:
    """
    Get usage examples for a specific tool.

    Args:
        tool_name: Name of the tool

    Returns:
        List of example dictionaries
    """
    try:
        metadata = get_tool_metadata(tool_name)
        return metadata.get("examples", [])
    except Exception as e:
        logger.error(f"Error getting examples for tool {tool_name}: {e}")
        return []


def get_tool_description(tool_name: str) -> str:
    """
    Get description for a specific tool.

    Args:
        tool_name: Name of the tool

    Returns:
        Tool description
    """
    try:
        metadata = get_tool_metadata(tool_name)
        return metadata.get("description", f"Execute {tool_name}")
    except Exception as e:
        logger.error(f"Error getting description for tool {tool_name}: {e}")
        return f"Execute {tool_name}"


def get_tool_input_schema(tool_name: str) -> Dict[str, Any]:
    """
    Get input schema for a specific tool.

    Args:
        tool_name: Name of the tool

    Returns:
        Tool input schema
    """
    try:
        metadata = get_tool_metadata(tool_name)
        return metadata.get("input_schema", {})
    except Exception as e:
        logger.error(f"Error getting input schema for tool {tool_name}: {e}")
        return {}
