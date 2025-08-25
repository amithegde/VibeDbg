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
            "dx_visualization": create_dx_visualization(command_executor),
            "load_symbols": create_load_symbols(command_executor),
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
            load_symbols = args.get("load_symbols", False) # New parameter

            analysis_commands = {
                "full": [
                    ("|", "Process and thread status"),
                    ("bl", "Breakpoint list"),
                    ("lm", "Loaded modules"),
                    ("~", "Thread list"),
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
                "parent_process": [
                    ("getparentprocess", "Parent process information"),
                ],
            }

            # Add load_symbols command to the full analysis if requested
            if load_symbols:
                analysis_commands["full"].append(("loadallsymbols", "Load all symbols"))

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
                        error_msg = f"{description} ({cmd}): Error - {result.error_message}"
                        
                        # Add helpful guidance for parent process analysis
                        if cmd == "getparentprocess" and ("symbols" in result.error_message.lower() or "symbol" in result.error_message.lower()):
                            error_msg += "\n\n💡 Tip: Try loading user-mode symbols first using the 'load_symbols' tool with symbol_type='user'."
                        
                        results.append(error_msg)
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


def create_dx_visualization(command_executor: CommandExecutor):
    """Create dx_visualization tool function."""

    async def dx_visualization(args: Dict[str, Any]) -> str:
        """Display debugger object model expressions using the NatVis extension model.

        This tool provides rich visualization of C++ objects, data structures, and
        debugger objects with customizable formatting options.
        """
        try:
            expression = args.get("expression", "")
            options = args.get("options", {})
            timeout = args.get("timeout", 30000)

            if not expression:
                return "Error: No expression specified"

            # Build the dx command with options
            dx_command = "dx"
            
            # Add grid option
            if options.get("grid", False):
                dx_command += " -g"
                
            # Add grid cell size
            grid_cell_size = options.get("grid_cell_size")
            if grid_cell_size is not None:
                dx_command += f" -gc {grid_cell_size}"
                
            # Add container skip
            container_skip = options.get("container_skip")
            if container_skip is not None:
                dx_command += f" -c {container_skip}"
                
            # Add native only flag
            if options.get("native_only", False):
                dx_command += " -n"
                
            # Add verbose flag
            if options.get("verbose", False):
                dx_command += " -v"
                
            # Add recursion level
            recursion_level = options.get("recursion_level", 1)
            if recursion_level > 1:
                dx_command += f" -r{recursion_level}"
                
            # Add format specifier
            format_specifier = options.get("format_specifier")
            if format_specifier:
                dx_command += f" {expression},{format_specifier}"
            else:
                dx_command += f" {expression}"

            # Execute the dx command
            result = await command_executor.execute_command(dx_command, timeout_ms=timeout)

            if result.success:
                if result.output and result.output.strip():
                    return f"dx Visualization ({expression}):\n{result.output.strip()}"
                else:
                    return f"dx Visualization ({expression}): Command executed successfully"
            else:
                error_msg = f"Error executing dx command for expression '{expression}': {result.error_message}"
                if result.output and result.output.strip():
                    error_msg += f"\nOutput: {result.output.strip()}"
                return error_msg

        except ValueError as e:
            logger.error(f"Invalid dx visualization parameters: {e}")
            return f"Error: Invalid dx visualization parameters - {str(e)}"
        except Exception as e:
            logger.error(f"Unexpected error in dx_visualization tool: {e}", exc_info=True)
            return f"Error: Unexpected error - {str(e)}"

    return dx_visualization


def create_load_symbols(command_executor: CommandExecutor):
    """Create load_symbols tool function."""

    async def load_symbols(args: Dict[str, Any]) -> str:
        """Load symbols for debugging operations.

        This tool loads user-mode symbols for comprehensive debugging capabilities.

        Args:
            args: Dictionary containing optional parameters:
                - symbol_type: Type of symbols to load - "user" for user-mode symbols only, 
                  "all" for comprehensive symbols (default: "all")
                - timeout: Command timeout in milliseconds (default: 30000)

        Returns:
            Result of symbol loading operation
        """
        try:
            symbol_type = args.get("symbol_type", "all")  # "user" or "all"
            timeout = args.get("timeout", 30000)

            # Choose command based on symbol type
            if symbol_type == "user":
                command = "loadusersymbols"
                description = "user-mode symbols"
            else:
                command = "loadallsymbols"
                description = "all symbols"

            # Execute the symbol loading command
            result = await command_executor.execute_command(command, timeout_ms=timeout)

            if result.success:
                if result.output and result.output.strip():
                    return result.output.strip()
                else:
                    return f"{description.capitalize()} loaded successfully"
            else:
                error_msg = f"Error loading {description}: {result.error_message}"
                if result.output and result.output.strip():
                    error_msg += f"\nOutput: {result.output.strip()}"
                return error_msg

        except ValueError as e:
            logger.error(f"Invalid load_symbols parameters: {e}")
            return f"Error: Invalid load_symbols parameters - {str(e)}"
        except Exception as e:
            logger.error(f"Unexpected error in load_symbols tool: {e}", exc_info=True)
            return f"Error: Unexpected error - {str(e)}"

    return load_symbols



