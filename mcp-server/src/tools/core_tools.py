"""
Core debugging tools for VibeDbg MCP Server.

This module provides the primary tools for executing WinDbg commands,
managing breakpoints, stepping through code, and analyzing debugging context.
"""

import logging
from typing import Dict, Any, List

from ..core.command_executor import CommandExecutor
from ..config import (
    INCLUDE_COMMAND_CONTEXT_IN_ERRORS,
    INCLUDE_COMMAND_CONTEXT_IN_SEQUENCES,
    INCLUDE_COMMAND_CONTEXT_IN_COMPLEX_COMMANDS,
    COMPLEX_COMMANDS_THAT_NEED_CONTEXT,
)

logger = logging.getLogger(__name__)


def get_core_tools(command_executor: CommandExecutor) -> Dict[str, Any]:
    """Get all core debugging tools.

    Args:
        command_executor: The command executor instance.

    Returns:
        Dictionary mapping tool names to tool functions.
    """
    try:
        return {
            "execute_command": create_execute_command(command_executor),
            "execute_sequence": create_execute_sequence(command_executor),
            "set_breakpoint": create_set_breakpoint(command_executor),
            "step_and_analyze": create_step_and_analyze(command_executor),
            "analyze_context": create_analyze_context(command_executor),
            "get_session_status": create_get_session_status(command_executor),
        }
    except AttributeError as e:
        logger.error(f"Invalid command executor provided: {e}")
        raise ValueError(f"Invalid command executor: {e}")
    except Exception as e:
        logger.error(f"Unexpected error creating core tools: {e}", exc_info=True)
        raise RuntimeError(f"Failed to create core tools: {e}")


def create_execute_command(command_executor: CommandExecutor):
    """Create execute_command tool function."""

    async def execute_command(args: Dict[str, Any]) -> str:
        """Execute any WinDbg command.

        This is the primary tool for executing specific WinDbg commands like:
        - Memory examination: da, db, dq, dps
        - Register inspection: r, rM
        - Disassembly: u, uf
        - Symbol operations: x, ln, .sympath
        - Module listing: lm, lmf
        - Process/thread info: |, ~
        """
        try:
            command = args.get("command", "")
            timeout = args.get("timeout", 30000)

            if not command:
                return "Error: No command specified"

            result = await command_executor.execute_command(command, timeout_ms=timeout)

            if result.success:
                # For successful commands, return raw output for simple commands
                # but include context for complex commands based on configuration
                if result.output and result.output.strip():
                    # Check if this is a complex command that benefits from context
                    command_lower = command.strip().lower()
                    is_complex = any(
                        cmd in command_lower
                        for cmd in COMPLEX_COMMANDS_THAT_NEED_CONTEXT
                    )

                    if is_complex and INCLUDE_COMMAND_CONTEXT_IN_COMPLEX_COMMANDS:
                        return f"Command: {command}\nOutput:\n{result.output.strip()}"
                    else:
                        return result.output.strip()
                else:
                    return "Command executed successfully"
            else:
                # For errors, include command context and recovery suggestions if configured
                error_msg = ""
                if INCLUDE_COMMAND_CONTEXT_IN_ERRORS:
                    error_msg = (
                        f"Error executing command '{command}': {result.error_message}"
                    )
                else:
                    error_msg = f"Error: {result.error_message}"

                if result.output and result.output.strip():
                    error_msg += f"\nOutput: {result.output.strip()}"

                # Add recovery suggestions for communication errors
                if "Communication error" in (result.error_message or ""):
                    recovery_suggestion = command_executor.suggest_recovery_action()
                    if recovery_suggestion:
                        error_msg += f"\n\nSuggestion: {recovery_suggestion}"
                elif "Connection lost but recovered" in (result.error_message or ""):
                    error_msg += "\n\nℹ️ The connection has been automatically restored - you can retry your command now."

                return error_msg
        except ValueError as e:
            logger.error(f"Invalid command or parameters in execute_command tool: {e}")
            return f"Error: Invalid command or parameters - {str(e)}"
        except Exception as e:
            logger.error(
                f"Unexpected error in execute_command tool: {e}", exc_info=True
            )
            return f"Error: Unexpected error - {str(e)}"

    return execute_command


def create_execute_sequence(command_executor: CommandExecutor):
    """Create execute_sequence tool function."""

    async def execute_sequence(args: Dict[str, Any]) -> str:
        """Execute multiple WinDbg commands in sequence.

        Perfect for complex debugging workflows like:
        - Examine function parameters then step through
        - Set breakpoint, run, then analyze crash
        - Multiple memory dumps for comparison
        """
        try:
            commands = args.get("commands", [])
            stop_on_error = args.get("stop_on_error", False)
            timeout_per_command = args.get("timeout_per_command", 30000)

            if not commands:
                return "Error: No commands specified"

            if not isinstance(commands, list):
                return "Error: Commands must be a list"

            results = []
            for i, command in enumerate(commands):
                try:
                    result = await command_executor.execute_command(
                        command, timeout_ms=timeout_per_command
                    )

                    if result.success:
                        output = (
                            result.output.strip()
                            if result.output
                            else "Command executed successfully"
                        )
                        if INCLUDE_COMMAND_CONTEXT_IN_SEQUENCES:
                            results.append(f"[{i+1}] {command}: {output}")
                        else:
                            results.append(output)
                    else:
                        error_msg = f"Command {i+1} failed: {result.error_message}"
                        if result.output and result.output.strip():
                            error_msg += f"\nOutput: {result.output.strip()}"

                        # For communication errors in sequences, provide better guidance
                        if "Connection lost" in (result.error_message or ""):
                            # Connection issues are handled in the sequence logic
                            results.append(error_msg)
                            break  # Stop on connection issues
                        elif "Communication error" in (result.error_message or ""):
                            if (
                                "pipe is being closed"
                                in (result.error_message or "").lower()
                            ):
                                error_msg += (
                                    "\n(Connection issue detected - sequence stopped)"
                                )
                                results.append(error_msg)
                                break  # Always stop on pipe errors

                        if stop_on_error:
                            results.append(error_msg)
                            break
                        else:
                            results.append(error_msg)

                except ValueError as e:
                    error_msg = (
                        f"Command {i+1} failed with invalid parameters: {str(e)}"
                    )
                    if stop_on_error:
                        results.append(error_msg)
                        break
                    else:
                        results.append(error_msg)
                except Exception as e:
                    logger.error(
                        f"Unexpected error in command {i+1} ('{command}'): {e}",
                        exc_info=True,
                    )
                    error_msg = f"Command {i+1} failed with unexpected error: {str(e)}"
                    if stop_on_error:
                        results.append(error_msg)
                        break
                    else:
                        results.append(error_msg)

            return "\n".join(results)
        except ValueError as e:
            logger.error(f"Invalid parameters in execute_sequence tool: {e}")
            return f"Error: Invalid parameters - {str(e)}"
        except Exception as e:
            logger.error(
                f"Unexpected error in execute_sequence tool: {e}", exc_info=True
            )
            return f"Error: Unexpected error - {str(e)}"

    return execute_sequence


def create_set_breakpoint(command_executor: CommandExecutor):
    """Create set_breakpoint tool function."""

    async def set_breakpoint(args: Dict[str, Any]) -> str:
        """Set a breakpoint at a specific location.

        Args:
            location: Address, symbol, or expression where to set breakpoint
            condition: Optional condition for conditional breakpoint
            command: Optional command to execute when breakpoint hits
        """
        try:
            location = args.get("location", "")
            condition = args.get("condition", "")
            command = args.get("command", "")

            if not location:
                return "Error: No breakpoint location specified"

            # Build the breakpoint command
            bp_command = f"bp {location}"
            if condition:
                bp_command += f" {condition}"
            if command:
                bp_command += f' "{command}"'

            result = await command_executor.execute_command(bp_command)

            if result.success:
                return f"Breakpoint set successfully at {location}\n{result.output.strip()}"
            else:
                return f"Failed to set breakpoint: {result.error_message}"
        except ValueError as e:
            logger.error(f"Invalid breakpoint parameters: {e}")
            return f"Error: Invalid breakpoint parameters - {str(e)}"
        except Exception as e:
            logger.error(f"Unexpected error in set_breakpoint tool: {e}", exc_info=True)
            return f"Error: Unexpected error - {str(e)}"

    return set_breakpoint


def create_step_and_analyze(command_executor: CommandExecutor):
    """Create step_and_analyze tool function."""

    async def step_and_analyze(args: Dict[str, Any]) -> str:
        """Step through code and analyze the current state.

        This tool combines stepping with analysis to provide context
        about what happened during the step.
        """
        try:
            step_type = args.get("step_type", "over")  # over, into, out
            analysis_depth = args.get("analysis_depth", "basic")  # basic, detailed

            # Execute the step command
            step_commands = {"over": "p", "into": "t", "out": "gu"}

            step_cmd = step_commands.get(step_type, "p")
            step_result = await command_executor.execute_command(step_cmd)

            if not step_result.success:
                return f"Step failed: {step_result.error_message}"

            # Analyze the current state
            analysis_commands = []
            if analysis_depth == "detailed":
                analysis_commands = [
                    "k",  # Stack trace
                    "r",  # Registers
                    "dv",  # Local variables
                    "dt",  # Display type
                ]
            else:
                analysis_commands = [
                    "k",  # Stack trace
                    "r",  # Registers
                ]

            analysis_results = []
            for cmd in analysis_commands:
                try:
                    result = await command_executor.execute_command(cmd)
                    if result.success and result.output:
                        analysis_results.append(f"{cmd}:\n{result.output.strip()}")
                except ValueError as e:
                    analysis_results.append(f"{cmd}: Invalid parameters - {str(e)}")
                except Exception as e:
                    logger.error(
                        f"Unexpected error in step analysis command '{cmd}': {e}",
                        exc_info=True,
                    )
                    analysis_results.append(f"{cmd}: Unexpected error - {str(e)}")

            # Combine results
            output = f"Step ({step_type}) completed successfully.\n\n"
            output += "\n\n".join(analysis_results)

            return output
        except ValueError as e:
            logger.error(f"Invalid step parameters: {e}")
            return f"Error: Invalid step parameters - {str(e)}"
        except Exception as e:
            logger.error(
                f"Unexpected error in step_and_analyze tool: {e}", exc_info=True
            )
            return f"Error: Unexpected error - {str(e)}"

    return step_and_analyze


def create_analyze_context(command_executor: CommandExecutor):
    """Create analyze_context tool function."""

    async def analyze_context(args: Dict[str, Any]) -> str:
        """Analyze the current debugging context.

        Provides a comprehensive view of the current state including
        process, thread, stack, registers, and memory.
        """
        try:
            analysis_type = args.get("type", "full")  # full, stack, memory, process

            analysis_commands = {
                "full": [
                    ("|", "Current process and thread"),
                    ("~", "All threads"),
                    ("k", "Current stack trace"),
                    ("r", "Current registers"),
                    ("lm", "Loaded modules"),
                ],
                "stack": [
                    ("k", "Stack trace"),
                    ("kb", "Stack trace with arguments"),
                    ("kp", "Stack trace with parameters"),
                ],
                "memory": [
                    ("r", "Registers"),
                    ("dps @esp", "Stack memory"),
                    ("dps @ebp", "Frame memory"),
                ],
                "process": [
                    ("|", "Current process and thread"),
                    ("~", "All threads"),
                    ("!process", "Process information"),
                ],
            }

            commands = analysis_commands.get(analysis_type, analysis_commands["full"])

            results = []
            for cmd, description in commands:
                try:
                    result = await command_executor.execute_command(cmd)
                    if result.success and result.output:
                        results.append(
                            f"{description} ({cmd}):\n{result.output.strip()}"
                        )
                    elif not result.success:
                        results.append(
                            f"{description} ({cmd}): Error - {result.error_message}"
                        )
                except ValueError as e:
                    results.append(
                        f"{description} ({cmd}): Invalid parameters - {str(e)}"
                    )
                except Exception as e:
                    logger.error(
                        f"Unexpected error in context analysis command '{cmd}': {e}",
                        exc_info=True,
                    )
                    results.append(
                        f"{description} ({cmd}): Unexpected error - {str(e)}"
                    )

            return f"Context Analysis ({analysis_type}):\n\n" + "\n\n".join(results)
        except ValueError as e:
            logger.error(f"Invalid context analysis parameters: {e}")
            return f"Error: Invalid context analysis parameters - {str(e)}"
        except Exception as e:
            logger.error(
                f"Unexpected error in analyze_context tool: {e}", exc_info=True
            )
            return f"Error: Unexpected error - {str(e)}"

    return analyze_context


def create_get_session_status(command_executor: CommandExecutor):
    """Create get_session_status tool function."""

    async def get_session_status(args: Dict[str, Any]) -> str:
        """Get the current debugging session status.

        Returns information about the current debugging session including
        process status, breakpoints, and session health.
        """
        try:
            status_commands = [
                ("|", "Process and thread status"),
                ("bl", "Breakpoint list"),
                ("lm", "Loaded modules"),
                ("~", "Thread list"),
            ]

            results = []
            for cmd, description in status_commands:
                try:
                    result = await command_executor.execute_command(cmd)
                    if result.success and result.output:
                        results.append(f"{description}:\n{result.output.strip()}")
                    elif not result.success:
                        results.append(f"{description}: Error - {result.error_message}")
                except ValueError as e:
                    results.append(f"{description}: Invalid parameters - {str(e)}")
                except Exception as e:
                    logger.error(
                        f"Unexpected error in session status command '{cmd}': {e}",
                        exc_info=True,
                    )
                    results.append(f"{description}: Unexpected error - {str(e)}")

            return "Session Status:\n\n" + "\n\n".join(results)
        except ValueError as e:
            logger.error(f"Invalid session status parameters: {e}")
            return f"Error: Invalid session status parameters - {str(e)}"
        except Exception as e:
            logger.error(
                f"Unexpected error in get_session_status tool: {e}", exc_info=True
            )
            return f"Error: Unexpected error - {str(e)}"

    return get_session_status
