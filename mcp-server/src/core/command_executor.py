"""
Command execution engine for VibeDbg MCP Server.

This module handles command execution, routing, and optimization.
"""

import asyncio
import logging
import time
import re
from typing import Optional, Dict, Any, List
from dataclasses import dataclass
from datetime import datetime

from .communication import CommunicationManager, CommunicationError, TimeoutError
from ..config import config, get_timeout_for_command

logger = logging.getLogger(__name__)

# ====================================================================
# DATA CLASSES
# ====================================================================


@dataclass
class ExecutionContext:
    """Context for command execution."""

    session_id: str
    current_process: Optional[str] = None
    current_thread: Optional[int] = None
    breakpoints: List[Dict[str, Any]] = None
    last_command: Optional[str] = None
    last_command_time: Optional[datetime] = None

    def __post_init__(self):
        if self.breakpoints is None:
            self.breakpoints = []


@dataclass
class CommandRoute:
    """Represents a command route to a specific handler."""

    handler_name: str
    parameters: Dict[str, Any]
    is_generic: bool = False
    confidence: float = 1.0


@dataclass
class CommandResult:
    """Result of command execution."""

    success: bool
    output: str
    error_message: Optional[str] = None
    execution_time_ms: int = 0
    command_executed: str = ""
    route_used: Optional[CommandRoute] = None
    cached: bool = False


# ====================================================================
# COMMAND ROUTING
# ====================================================================


class CommandRouter:
    """Routes WinDbg commands to appropriate handlers."""

    def __init__(self):
        self._command_patterns = {
            # Stack trace commands
            r"^k[lnpkv]?$": CommandRoute("analyze_stack", {}, False, 1.0),
            r"^kb$": CommandRoute("analyze_stack", {"show_args": True}, False, 1.0),
            r"^kp$": CommandRoute("analyze_stack", {"show_params": True}, False, 1.0),
            # Thread commands
            r"^~$": CommandRoute("list_threads", {}, False, 1.0),
            r"^~\d+s$": CommandRoute("switch_thread", {"thread_id": None}, False, 1.0),
            r"^~\d+k$": CommandRoute("thread_stack", {"thread_id": None}, False, 1.0),
            # Process commands
            r"^\.tlist$": CommandRoute("list_processes", {}, False, 1.0),
            r"^!process\s+0\s+0$": CommandRoute(
                "list_processes", {}, False, 1.0
            ),  # Keep for backward compatibility
            r"^!process\s+(\w+)\s+(\w+)$": CommandRoute(
                "process_info", {"process_id": None, "flags": None}, False, 1.0
            ),
            r"^\.attach\s+(\w+)$": CommandRoute(
                "attach_process", {"process_id": None}, False, 1.0
            ),
            r"^\.detach$": CommandRoute("detach_process", {}, False, 1.0),
            r"^\.create\s+(.+)$": CommandRoute(
                "create_process", {"process_path": None}, False, 1.0
            ),
            # Breakpoint commands
            r"^bp\s+(.+)$": CommandRoute(
                "set_breakpoint", {"location": None}, False, 1.0
            ),
            r"^bl$": CommandRoute("list_breakpoints", {}, False, 1.0),
            r"^bc\s+(\d+)$": CommandRoute(
                "clear_breakpoint", {"breakpoint_id": None}, False, 1.0
            ),
            r"^bd\s+(\d+)$": CommandRoute(
                "disable_breakpoint", {"breakpoint_id": None}, False, 1.0
            ),
            r"^be\s+(\d+)$": CommandRoute(
                "enable_breakpoint", {"breakpoint_id": None}, False, 1.0
            ),
            # Execution control
            r"^g$": CommandRoute("continue_execution", {}, False, 1.0),
            r"^p$": CommandRoute("step_over", {}, False, 1.0),
            r"^t$": CommandRoute("step_into", {}, False, 1.0),
            r"^gu$": CommandRoute("step_out", {}, False, 1.0),
            # Memory commands
            r"^d[bdwq]\s+([0-9a-fA-Fx]+)(?:\s+L([0-9a-fA-Fx]+))?$": CommandRoute(
                "read_memory", {"address": None, "size": None}, False, 1.0
            ),
            r"^e[bdwq]\s+([0-9a-fA-Fx]+)\s+(.+)$": CommandRoute(
                "write_memory", {"address": None, "value": None}, False, 1.0
            ),
            # Module commands
            r"^lm$": CommandRoute("list_modules", {}, False, 1.0),
            r"^lm\s+m\s+(.+)$": CommandRoute(
                "module_info", {"module_name": None}, False, 1.0
            ),
            # Register commands
            r"^r$": CommandRoute("show_registers", {}, False, 1.0),
            r"^r\s+(\w+)\s*=\s*(.+)$": CommandRoute(
                "set_register", {"register": None, "value": None}, False, 1.0
            ),
            # Analysis commands
            r"^!analyze\s+-v$": CommandRoute("analyze_crash", {}, False, 1.0),
            r"^!analyze\s+-f$": CommandRoute(
                "analyze_crash", {"force": True}, False, 1.0
            ),
            # Symbol commands
            r"^x\s+(.+)$": CommandRoute("symbol_search", {"pattern": None}, False, 1.0),
            r"^ln\s+([0-9a-fA-Fx]+)$": CommandRoute(
                "nearest_symbol", {"address": None}, False, 1.0
            ),
            # Variable commands
            r"^dv$": CommandRoute("list_variables", {}, False, 1.0),
            r"^dt\s+(.+)$": CommandRoute("dump_type", {"type_name": None}, False, 1.0),
            # Memory search and compare commands
            r"^s\s+-[abdwq]\s+([0-9a-fA-Fx]+)\s+([0-9a-fA-Fx]+)\s+(.+)$": CommandRoute(
                "search_memory",
                {"start_addr": None, "end_addr": None, "pattern": None},
                False,
                1.0,
            ),
            r"^c\s+([0-9a-fA-Fx]+)\s+([0-9a-fA-Fx]+)\s+([0-9a-fA-Fx]+)$": CommandRoute(
                "compare_memory",
                {"addr1": None, "addr2": None, "length": None},
                False,
                1.0,
            ),
            r"^m\s+([0-9a-fA-Fx]+)\s+([0-9a-fA-Fx]+)\s+([0-9a-fA-Fx]+)$": CommandRoute(
                "move_memory",
                {"source": None, "dest": None, "length": None},
                False,
                1.0,
            ),
            # Hardware breakpoint commands
            r"^ba\s+([rwex])\s*(\d+)?\s+([0-9a-fA-Fx]+)$": CommandRoute(
                "set_hardware_breakpoint",
                {"access_type": None, "size": None, "address": None},
                False,
                1.0,
            ),
            # Advanced stepping commands
            r"^pc$": CommandRoute("step_to_call", {}, False, 1.0),
            r"^pt$": CommandRoute("step_to_return", {}, False, 1.0),
            # String display commands
            r"^da\s+([0-9a-fA-Fx]+)(?:\s+L([0-9a-fA-Fx]+))?$": CommandRoute(
                "display_ascii", {"address": None, "length": None}, False, 1.0
            ),
            r"^du\s+([0-9a-fA-Fx]+)(?:\s+L([0-9a-fA-Fx]+))?$": CommandRoute(
                "display_unicode", {"address": None, "length": None}, False, 1.0
            ),
            # Disassembly commands
            r"^u\s+([0-9a-fA-Fx]+)(?:\s+L([0-9a-fA-Fx]+))?$": CommandRoute(
                "unassemble", {"address": None, "length": None}, False, 1.0
            ),
            r"^uf\s+([0-9a-fA-Fx]+|[\w!]+)$": CommandRoute(
                "unassemble_function", {"function": None}, False, 1.0
            ),
        }

    def route_command(self, command: str) -> CommandRoute:
        """
        Route a command to the best handler.

        Args:
            command: WinDbg command to route

        Returns:
            CommandRoute with handler and parameters
        """
        command = command.strip()

        # Check specific patterns first
        for pattern, route in self._command_patterns.items():
            match = re.match(pattern, command)
            if match:
                # Extract parameters from match groups
                params = route.parameters.copy()
                if match.groups():
                    for i, group in enumerate(match.groups()):
                        if group is not None:
                            # Map group to parameter based on position
                            param_names = list(params.keys())
                            if i < len(param_names):
                                params[param_names[i]] = group

                return CommandRoute(
                    route.handler_name, params, route.is_generic, route.confidence
                )

        # Fall back to generic execution
        return CommandRoute("execute_generic", {"command": command}, True, 0.5)


# ====================================================================
# COMMAND CACHING
# ====================================================================


class CommandCache:
    """Caches command results for performance."""

    def __init__(self, max_size: int = 100, ttl_seconds: int = 300):
        self._cache: Dict[str, Dict[str, Any]] = {}
        self._max_size = max_size
        self._ttl_seconds = ttl_seconds
        self._lock = asyncio.Lock()

    def _get_cache_key(self, command: str, context: ExecutionContext) -> str:
        """Generate cache key for command and context."""
        # Include relevant context in cache key (user-mode debugging only)
        context_str = f"usermode_{context.current_process}_{context.current_thread}"
        return f"{command}_{context_str}"

    async def get_cached_result(
        self, command: str, context: ExecutionContext
    ) -> Optional[CommandResult]:
        """Get cached result if available and valid."""
        if not config.enable_command_caching:
            return None

        async with self._lock:
            cache_key = self._get_cache_key(command, context)

            if cache_key in self._cache:
                cached_data = self._cache[cache_key]
                cache_time = cached_data["timestamp"]

                # Check if cache is still valid
                if time.time() - cache_time < self._ttl_seconds:
                    logger.debug(f"Using cached result for command: {command}")
                    return CommandResult(
                        success=cached_data["success"],
                        output=cached_data["output"],
                        error_message=cached_data.get("error_message"),
                        execution_time_ms=cached_data.get("execution_time_ms", 0),
                        command_executed=cached_data.get("command_executed", command),
                        cached=True,
                    )
                else:
                    # Remove expired cache entry
                    del self._cache[cache_key]

            return None

    async def cache_result(
        self, command: str, result: CommandResult, context: ExecutionContext
    ):
        """Cache command result."""
        if not config.enable_command_caching:
            return

        async with self._lock:
            # Clean up cache if it's too large
            if len(self._cache) >= self._max_size:
                self._cleanup_cache()

            cache_key = self._get_cache_key(command, context)
            self._cache[cache_key] = {
                "success": result.success,
                "output": result.output,
                "error_message": result.error_message,
                "execution_time_ms": result.execution_time_ms,
                "command_executed": result.command_executed,
                "timestamp": time.time(),
            }

    def _cleanup_cache(self):
        """Clean up old cache entries."""
        current_time = time.time()
        expired_keys = []

        for key, data in self._cache.items():
            if current_time - data["timestamp"] > self._ttl_seconds:
                expired_keys.append(key)

        for key in expired_keys:
            del self._cache[key]

        # If still too large, remove oldest entries
        if len(self._cache) >= self._max_size:
            sorted_items = sorted(self._cache.items(), key=lambda x: x[1]["timestamp"])
            items_to_remove = (
                len(sorted_items) - self._max_size + 10
            )  # Remove extra 10 items
            for key, _ in sorted_items[:items_to_remove]:
                del self._cache[key]


# ====================================================================
# COMMAND VALIDATION
# ====================================================================


class CommandValidator:
    """Validates commands for safety and correctness."""

    def __init__(self):
        self._dangerous_patterns = [
            r"\bformat\b",
            r"\bdel\b",
            r"\brmdir\b",
            r"\bdelete\b",
            r"\bremove\b",
            r"\bkill\b",
            r"\bshutdown\b",
            r"\breboot\b",
            # Note: Removed exit and quit - these are legitimate WinDbg commands
        ]

        self._safe_commands = {
            "version",
            "help",
            "?",
            "cls",
            "clear",
            "k",
            "kb",
            "kp",
            "kn",
            "kv",
            "~",
            "r",
            "lm",
            "x",
            "ln",
            "dv",
            "dt",
            "bp",
            "bl",
            "bc",
            "bd",
            "be",
            "ba",  # Hardware breakpoints
            "g",
            "p",
            "t",
            "gu",
            "pc",  # Step to next call
            "pt",  # Step to next return
            "db",
            "dw",
            "dd",
            "dq",
            "da",  # Display ASCII strings
            "du",  # Display Unicode strings
            "eb",
            "ew",
            "ed",
            "eq",
            "s",  # Search memory
            "c",  # Compare memory
            "m",  # Move memory
            "u",  # Unassemble
            "uf",  # Unassemble function
            "!analyze",
            "!dump",
            "!locks",
            "!handle",
            "!peb",
            "!teb",
            "vertarget",
            "|",  # Process/thread context command
            ".restart",
            ".attach",
            ".detach",
            ".create",
            ".cls",
            ".break",
            ".time",
            ".lastevent",
            "~*k",  # All thread stacks
            "~*",  # All thread info
            "q",  # Quit WinDbg (legitimate debugging command)
            "qq",  # Quit without confirmation
            "qd",  # Quit and detach
            "ctrl+c",  # Break execution
            "ctrl+break",  # Break execution
            "sxe",  # Set exception handling
            "sxi",  # Set exception handling
            "sxd",  # Set exception handling
        }

    def is_safe_command(self, command: str) -> bool:
        """Check if a command is safe for execution."""
        if not config.enable_command_validation:
            return True

        command_lower = command.lower().strip()

        # Check for dangerous patterns, but allow debugging-specific commands
        debugging_contexts = [
            "windbg",
            "debug",
            "bp",
            "!analyze",
            "sxe",
            "sxi",
            "~",
            "|",
        ]
        is_debugging_context = any(ctx in command_lower for ctx in debugging_contexts)

        for pattern in self._dangerous_patterns:
            if re.search(pattern, command_lower):
                # Allow if it's in a debugging context
                if not is_debugging_context:
                    return False

        # Check if it's a known safe command
        base_command = command_lower.split()[0] if command_lower else ""
        if base_command in self._safe_commands:
            return True

        # Special handling for debugging control commands
        if command_lower in ["q", "qq", "qd", "ctrl+c", "ctrl+break"]:
            return True

        # Allow exception handling commands (sx series)
        if command_lower.startswith(("sxe", "sxi", "sxd", "sxr", "sxn")):
            return True

        # Allow commands that start with common WinDbg prefixes
        safe_prefixes = [
            "!",
            ".",
            "k",
            "~",
            "r",
            "lm",
            "x",
            "ln",
            "dv",
            "dt",
            "bp",
            "bl",
            "bc",
            "bd",
            "be",
            "ba",  # Hardware breakpoints
            "g",
            "p",
            "t",
            "gu",
            "pc",  # Step to call
            "pt",  # Step to return
            "d",
            "e",
            "ver",
            "|",  # Process/thread context
            "s",  # Search commands
            "u",  # Unassemble commands
            "uf",  # Unassemble function commands
            "a",  # Assemble commands
            "~*",  # Thread operations
            "sx",  # Exception handling commands
            "q",  # Quit commands
            "c",  # Compare memory commands
            "m",  # Move memory commands
        ]
        if any(command_lower.startswith(prefix) for prefix in safe_prefixes):
            return True

        return False

    def validate_command_syntax(self, command: str) -> bool:
        """Validate basic command syntax."""
        if not command or not command.strip():
            return False

        # Basic syntax checks
        command = command.strip()

        # Check for balanced quotes
        if command.count('"') % 2 != 0:
            return False

        # Check for balanced parentheses
        if command.count("(") != command.count(")"):
            return False

        return True


# ====================================================================
# MAIN COMMAND EXECUTOR
# ====================================================================


class CommandExecutor:
    """Main command execution engine."""

    def __init__(self):
        self.comm_manager = CommunicationManager()
        self.router = CommandRouter()
        self.cache = CommandCache()
        self.validator = CommandValidator()
        self.context = ExecutionContext(session_id=f"session_{int(time.time())}")
        self._lock = asyncio.Lock()
        self._last_error_time = None
        self._consecutive_communication_errors = 0
        self._max_consecutive_errors = 3

    async def initialize(self) -> None:
        """Initialize the command executor."""
        try:
            # Test connection to WinDbg extension
            if not self.comm_manager.test_connection():
                raise CommunicationError("Failed to connect to WinDbg extension")

            # Update context with initial state
            await self._update_context()

            logger.info("Command executor initialized successfully")
        except CommunicationError:
            logger.error("Failed to initialize command executor: communication error")
            raise
        except Exception as e:
            logger.error(f"Failed to initialize command executor: {e}", exc_info=True)
            raise CommunicationError(f"Initialization failed: {e}")

    async def shutdown(self) -> None:
        """Shutdown the command executor."""
        try:
            self.comm_manager.cleanup()
            logger.info("Command executor shutdown complete")
        except Exception as e:
            logger.error(f"Error during command executor shutdown: {e}", exc_info=True)
            # Don't re-raise during shutdown to avoid masking other issues

    async def execute_command(
        self, command: str, timeout_ms: Optional[int] = None
    ) -> CommandResult:
        """
        Execute a WinDbg command with proper timeout and logging.

        Args:
            command: WinDbg command to execute
            timeout_ms: Command timeout (default: 30000ms)

        Returns:
            CommandResult with execution details
        """
        start_time = time.time()
        timeout = timeout_ms or 30000  # Default 30 second timeout

        logger.info(
            f"[CMD_EXEC] Starting command execution: '{command}' (timeout: {timeout}ms)"
        )

        # Check if this is a state-dependent command (requires target to be broken)
        state_dependent_commands = {
            "k",
            "r",
            "dv",
            "p",
            "t",
            "gu",
            "pc",
            "pt",
            "~*k",
            "u",
            "uf",
        }
        # Some commands work in both states
        context_commands = {
            "~",
            "|",
            "bl",
            "lm",
            "ba",
        }  # These work when running or broken

        command_lower = command.strip().lower()
        is_state_dependent = any(
            command_lower.startswith(cmd) for cmd in state_dependent_commands
        )
        is_context_command = any(
            command_lower.startswith(cmd) for cmd in context_commands
        )

        # Validate command
        if not self.validator.validate_command_syntax(command):
            logger.warning(
                f"[CMD_EXEC] Command validation failed - invalid syntax: '{command}'"
            )
            return CommandResult(
                success=False,
                output="",
                error_message="Invalid command syntax",
                execution_time_ms=0,
                command_executed=command,
            )

        if not self.validator.is_safe_command(command):
            logger.warning(
                f"[CMD_EXEC] Command validation failed - unsafe command: '{command}'"
            )
            return CommandResult(
                success=False,
                output="",
                error_message="Command is not safe for execution",
                execution_time_ms=0,
                command_executed=command,
            )

        # Check cache first
        cached_result = await self.cache.get_cached_result(command, self.context)
        if cached_result:
            logger.debug(f"[CMD_EXEC] Using cached result for: '{command}'")
            return cached_result

        # Route command
        route = self.router.route_command(command)
        logger.debug(
            f"[CMD_EXEC] Routed command '{command}' to handler '{route.handler_name}' (generic: {route.is_generic})"
        )

        try:
            # Execute with timeout using asyncio.wait_for
            if route.is_generic:
                result = await asyncio.wait_for(
                    self._execute_generic_command(command, timeout, route),
                    timeout=timeout / 1000,  # Convert to seconds
                )
            else:
                result = await asyncio.wait_for(
                    self._execute_routed_command(route, command, timeout),
                    timeout=timeout / 1000,  # Convert to seconds
                )
        except asyncio.TimeoutError:
            execution_time_ms = int((time.time() - start_time) * 1000)
            logger.error(f"[CMD_EXEC] Command timed out after {timeout}ms: '{command}'")
            return CommandResult(
                success=False,
                output="",
                error_message=f"Command execution timed out after {timeout}ms",
                execution_time_ms=execution_time_ms,
                command_executed=command,
            )
        except CommunicationError as e:
            execution_time_ms = int((time.time() - start_time) * 1000)
            error_message = str(e)

            # Check if this is a pipe closure that might be recoverable
            if (
                "pipe is being closed" in error_message.lower()
                or "232" in error_message
            ):
                logger.warning(
                    f"[CMD_EXEC] Pipe connection lost for command '{command}', attempting recovery..."
                )

                # Try to force reconnection
                if self.comm_manager.force_reconnect():
                    logger.info(
                        f"[CMD_EXEC] Successfully reconnected, will provide guidance to retry"
                    )
                    return CommandResult(
                        success=False,
                        output="",
                        error_message="Connection lost but recovered. The WinDbg extension is reconnected - please retry your command.",
                        execution_time_ms=execution_time_ms,
                        command_executed=command,
                    )
                else:
                    logger.error(f"[CMD_EXEC] Failed to reconnect to WinDbg extension")
                    return CommandResult(
                        success=False,
                        output="",
                        error_message="Connection lost to WinDbg extension. Check that WinDbg is running and the extension is loaded with '!vibedbg_status'.",
                        execution_time_ms=execution_time_ms,
                        command_executed=command,
                    )

            # For state-dependent commands, provide more context when they fail
            if is_state_dependent and "pipe is being closed" in error_message.lower():
                logger.warning(
                    f"[CMD_EXEC] State-dependent command failed due to debugger state change: '{command}'"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message="Command failed: Debugger state changed (target may be running or disconnected)",
                    execution_time_ms=execution_time_ms,
                    command_executed=command,
                )

            logger.error(
                f"[CMD_EXEC] Communication error executing command '{command}': {error_message}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Communication error: {error_message}",
                execution_time_ms=execution_time_ms,
                command_executed=command,
            )
        except ValueError as e:
            execution_time_ms = int((time.time() - start_time) * 1000)
            logger.error(
                f"[CMD_EXEC] Invalid command or parameters '{command}': {str(e)}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Invalid command or parameters: {str(e)}",
                execution_time_ms=execution_time_ms,
                command_executed=command,
            )
        except Exception as e:
            execution_time_ms = int((time.time() - start_time) * 1000)
            logger.error(
                f"[CMD_EXEC] Unexpected error executing command '{command}': {str(e)}",
                exc_info=True,
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Unexpected error: {str(e)}",
                execution_time_ms=execution_time_ms,
                command_executed=command,
            )

        # Calculate execution time
        execution_time_ms = int((time.time() - start_time) * 1000)
        result.execution_time_ms = execution_time_ms
        result.route_used = route

        logger.info(
            f"[CMD_EXEC] Command completed: '{command}' - Success: {result.success}, Time: {execution_time_ms}ms"
        )
        if not result.success:
            logger.error(f"[CMD_EXEC] Command error: {result.error_message}")
        else:
            logger.debug(
                f"[CMD_EXEC] Command output length: {len(result.output)} chars"
            )
            logger.debug(
                f"[CMD_EXEC] Command output: {result.output[:500]}{'...' if len(result.output) > 500 else ''}"
            )

        # Cache successful results
        if result.success:
            await self.cache.cache_result(command, result, self.context)

        # Update error tracking
        if result.success:
            self._consecutive_communication_errors = 0
        elif "Communication error" in (result.error_message or ""):
            # Don't increment if this was a recovered connection
            if "Connection lost but recovered" not in (result.error_message or ""):
                self._consecutive_communication_errors += 1
                self._last_error_time = datetime.now()

                # Log warning if we're having persistent communication issues
                if (
                    self._consecutive_communication_errors
                    >= self._max_consecutive_errors
                ):
                    logger.warning(
                        f"[CMD_EXEC] {self._consecutive_communication_errors} consecutive communication errors detected. "
                        "Attempting automatic recovery..."
                    )

        # Update context
        self.context.last_command = command
        self.context.last_command_time = datetime.now()

        return result

    async def _execute_generic_command(
        self, command: str, timeout_ms: Optional[int], route: CommandRoute
    ) -> CommandResult:
        """Execute a generic command directly."""
        timeout = timeout_ms or get_timeout_for_command(command)

        # Check if this is a state-dependent command (requires target to be broken)
        state_dependent_commands = {
            "k",
            "r",
            "dv",
            "p",
            "t",
            "gu",
            "pc",
            "pt",
            "~*k",
            "u",
            "uf",
        }
        # Some commands work in both states
        context_commands = {
            "~",
            "|",
            "bl",
            "lm",
            "q",
            "qq",
            "qd",
            "ba",
        }  # These work when running or broken
        # Exception handling commands also work in both states
        exception_commands = {"sxe", "sxi", "sxd", "sxr", "sxn"}

        command_lower = command.strip().lower()
        is_state_dependent = any(
            command_lower.startswith(cmd) for cmd in state_dependent_commands
        )
        is_context_command = any(
            command_lower.startswith(cmd) for cmd in context_commands
        )

        try:
            output = self.comm_manager.send_command(command, timeout)

            # Check if output indicates an error - but handle state-dependent commands specially
            serious_error_indicators = [
                "invalid command",
                "unknown command",
                "syntax error",
                "access violation",
                "exception occurred",
                "debuggee not connected",
                "no current process",
            ]

            # Generic "command execution failed" often means target state issue, not command error
            generic_execution_failed = (
                "error in command execution: command execution failed"
            )

            output_lower = output.lower() if output else ""
            has_serious_error = any(
                indicator in output_lower for indicator in serious_error_indicators
            )
            has_generic_error = generic_execution_failed in output_lower

            if output and has_serious_error:
                logger.warning(
                    f"[CMD_EXEC] Command output contains serious error indicators: '{command}' -> {output[:200]}"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command failed: {output[:200]}",
                    command_executed=command,
                )
            elif output and has_generic_error and is_state_dependent:
                # For state-dependent commands with generic error, this usually means target is running
                logger.info(
                    f"[CMD_EXEC] State-dependent command failed (target likely running): '{command}'"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command unavailable: Target appears to be running (not in break state). Use Ctrl+Break in WinDbg to break execution.",
                    command_executed=command,
                )
            elif output and has_generic_error:
                # For non-state-dependent commands, generic error is still a failure
                logger.warning(
                    f"[CMD_EXEC] Command failed with generic error: '{command}' -> {output[:200]}"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command failed: {output[:200]}",
                    command_executed=command,
                )

            # For execution control commands, empty output is expected and successful
            execution_control_commands = {
                "g",
                "gh",
                "gn",
                "gu",
                "p",
                "t",
                "bp",
                "ba",
                "bu",
                "bm",
                "bc",
                "bd",
                "be",
                ".restart",
                ".attach",
                ".detach",
            }
            command_lower = command.strip().lower()
            is_execution_control = any(
                command_lower.startswith(cmd) for cmd in execution_control_commands
            )

            if not output or output.strip() == "":
                if is_execution_control:
                    # Execution control commands often have empty output when successful
                    logger.debug(
                        f"[CMD_EXEC] Execution control command completed: '{command}'"
                    )
                    return CommandResult(
                        success=True,
                        output="Command executed successfully",
                        command_executed=command,
                    )
                else:
                    # For state-dependent commands, empty output might indicate target is running
                    if is_state_dependent:
                        logger.info(
                            f"[CMD_EXEC] State-dependent command returned empty output (target may be running): '{command}'"
                        )
                        return CommandResult(
                            success=False,
                            output="",
                            error_message="Command unavailable: Target may be running or not in break state",
                            command_executed=command,
                        )
                    else:
                        logger.warning(
                            f"[CMD_EXEC] Command returned empty output: '{command}'"
                        )
                        return CommandResult(
                            success=False,
                            output="",
                            error_message="Command returned empty output",
                            command_executed=command,
                        )

            # Return the actual WinDbg output
            return CommandResult(
                success=True, output=output.strip(), command_executed=command
            )
        except CommunicationError as e:
            logger.error(
                f"[CMD_EXEC] Communication error during generic command execution: {str(e)}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Communication error: {str(e)}",
                command_executed=command,
            )
        except ValueError as e:
            logger.error(
                f"[CMD_EXEC] Invalid command or parameters during generic execution: {str(e)}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Invalid command or parameters: {str(e)}",
                command_executed=command,
            )
        except Exception as e:
            logger.error(
                f"[CMD_EXEC] Unexpected error during generic command execution: {str(e)}",
                exc_info=True,
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Unexpected error: {str(e)}",
                command_executed=command,
            )

    async def _execute_routed_command(
        self, route: CommandRoute, original_command: str, timeout_ms: Optional[int]
    ) -> CommandResult:
        """Execute a command using a specific handler."""
        timeout = timeout_ms or get_timeout_for_command(original_command)

        try:
            # Execute the command and get raw output
            output = self.comm_manager.send_command(original_command, timeout)

            # Check if output indicates an error even if no exception was thrown
            # Use same logic as generic command handling
            serious_error_indicators = [
                "invalid command",
                "unknown command",
                "syntax error",
                "access violation",
                "exception occurred",
                "debuggee not connected",
                "no current process",
            ]

            generic_execution_failed = (
                "error in command execution: command execution failed"
            )
            output_lower = output.lower() if output else ""
            has_serious_error = any(
                indicator in output_lower for indicator in serious_error_indicators
            )
            has_generic_error = generic_execution_failed in output_lower

            # Check if this is a state-dependent command
            state_dependent_commands = {
                "k",
                "r",
                "dv",
                "p",
                "t",
                "gu",
                "pc",
                "pt",
                "~*k",
                "u",
                "uf",
            }
            context_commands = {"~", "|", "bl", "lm", "q", "qq", "qd", "ba"}
            exception_commands = {"sxe", "sxi", "sxd", "sxr", "sxn"}

            command_lower = original_command.strip().lower()
            is_routed_state_dependent = any(
                command_lower.startswith(cmd) for cmd in state_dependent_commands
            )
            is_routed_context_command = any(
                command_lower.startswith(cmd) for cmd in context_commands
            )
            is_routed_exception_command = any(
                command_lower.startswith(cmd) for cmd in exception_commands
            )

            if output and has_serious_error:
                logger.warning(
                    f"[CMD_EXEC] Command output contains serious error indicators: '{original_command}' -> {output[:200]}"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command failed: {output[:200]}",
                    command_executed=original_command,
                )
            elif output and has_generic_error and is_routed_state_dependent:
                # For state-dependent commands with generic error, this usually means target is running
                logger.info(
                    f"[CMD_EXEC] State-dependent routed command failed (target likely running): '{original_command}'"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command unavailable: Target appears to be running (not in break state). Use Ctrl+Break in WinDbg to break execution.",
                    command_executed=original_command,
                )
            elif output and has_generic_error:
                logger.warning(
                    f"[CMD_EXEC] Routed command failed with generic error: '{original_command}' -> {output[:200]}"
                )
                return CommandResult(
                    success=False,
                    output="",
                    error_message=f"Command failed: {output[:200]}",
                    command_executed=original_command,
                )

            # Check for empty output, but allow it for execution control commands
            execution_control_handlers = {
                "continue_execution",
                "step_over",
                "step_into",
                "step_out",
            }
            if not output or output.strip() == "":
                if route.handler_name in execution_control_handlers:
                    # Execution control commands often have empty output when successful
                    logger.debug(
                        f"[CMD_EXEC] Execution control command completed: '{original_command}'"
                    )
                    enhanced_output = self._enhance_response(
                        route, original_command, "Command executed successfully"
                    )
                    return CommandResult(
                        success=True,
                        output=enhanced_output,
                        command_executed=original_command,
                    )
                else:
                    logger.warning(
                        f"[CMD_EXEC] Routed command returned empty output: '{original_command}'"
                    )
                    return CommandResult(
                        success=False,
                        output="",
                        error_message="Command returned empty output",
                        command_executed=original_command,
                    )

            # Enhance output with minimal context
            enhanced_output = self._enhance_response(route, original_command, output)

            return CommandResult(
                success=True, output=enhanced_output, command_executed=original_command
            )

        except CommunicationError as e:
            logger.error(
                f"[CMD_EXEC] Communication error during routed command execution: {str(e)}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Communication error: {str(e)}",
                command_executed=original_command,
            )
        except ValueError as e:
            logger.error(
                f"[CMD_EXEC] Invalid command or parameters during routed execution: {str(e)}"
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Invalid command or parameters: {str(e)}",
                command_executed=original_command,
            )
        except Exception as e:
            logger.error(
                f"[CMD_EXEC] Unexpected error during routed command execution: {str(e)}",
                exc_info=True,
            )
            return CommandResult(
                success=False,
                output="",
                error_message=f"Unexpected error: {str(e)}",
                command_executed=original_command,
            )

    def _enhance_response(
        self, route: CommandRoute, original_command: str, output: str
    ) -> str:
        """Enhance response with clean, consistent formatting while preserving actual WinDbg output."""
        # Always preserve the actual WinDbg output
        if output and output.strip():
            # Return the raw WinDbg output - this is what the LLM needs to see
            return output.strip()

        # For commands with no output, provide a simple success message
        # but only for execution control commands that are expected to have no output
        execution_control_handlers = {
            "continue_execution",
            "step_over",
            "step_into",
            "step_out",
        }
        if route.handler_name in execution_control_handlers:
            return "Command executed successfully"
        else:
            # For other commands, return empty string to indicate no output
            return ""

    async def execute_sequence(
        self, commands: List[str], stop_on_error: bool = False
    ) -> List[CommandResult]:
        """
        Execute a sequence of commands with improved error handling.

        Args:
            commands: List of commands to execute
            stop_on_error: Whether to stop on first error

        Returns:
            List of command results
        """
        results = []
        consecutive_failures = 0
        max_consecutive_failures = 2

        for i, command in enumerate(commands):
            try:
                result = await self.execute_command(command)
                results.append(result)

                if result.success:
                    consecutive_failures = 0  # Reset on success
                else:
                    consecutive_failures += 1

                    # If we have too many consecutive failures and more commands to run,
                    # suggest stopping to avoid wasting time
                    if (
                        consecutive_failures >= max_consecutive_failures
                        and i < len(commands) - 1
                        and not stop_on_error
                    ):
                        logger.warning(
                            f"[CMD_EXEC] Multiple consecutive failures ({consecutive_failures}), "
                            f"consider checking debugger state"
                        )

                if stop_on_error and not result.success:
                    break

            except CommunicationError as e:
                consecutive_failures += 1
                error_str = str(e)

                # Check for pipe closure - try recovery
                if "pipe is being closed" in error_str.lower() or "232" in error_str:
                    logger.warning(
                        f"[CMD_EXEC] Pipe connection lost during sequence at command {i+1}: '{command}'"
                    )

                    # Try to recover
                    if self.comm_manager.force_reconnect():
                        logger.info(
                            "[CMD_EXEC] Reconnected successfully during sequence"
                        )
                        error_result = CommandResult(
                            success=False,
                            output="",
                            error_message=f"Command {i+1} failed due to connection loss, but connection recovered. Sequence stopped.",
                            execution_time_ms=0,
                            command_executed=command,
                        )
                        results.append(error_result)
                        break  # Stop sequence after recovery
                    else:
                        error_result = CommandResult(
                            success=False,
                            output="",
                            error_message=f"Command {i+1} failed: Connection lost and recovery failed. Check WinDbg extension.",
                            execution_time_ms=0,
                            command_executed=command,
                        )
                        results.append(error_result)
                        break  # Stop sequence on failed recovery
                else:
                    error_result = CommandResult(
                        success=False,
                        output="",
                        error_message=f"Communication error: {error_str}",
                        execution_time_ms=0,
                        command_executed=command,
                    )
                    results.append(error_result)

            except ValueError as e:
                consecutive_failures += 1
                error_result = CommandResult(
                    success=False,
                    output="",
                    error_message=f"Invalid command or parameters: {str(e)}",
                    execution_time_ms=0,
                    command_executed=command,
                )
                results.append(error_result)
            except Exception as e:
                consecutive_failures += 1
                logger.error(
                    f"Unexpected error in command sequence for '{command}': {e}",
                    exc_info=True,
                )
                error_result = CommandResult(
                    success=False,
                    output="",
                    error_message=f"Unexpected error: {str(e)}",
                    execution_time_ms=0,
                    command_executed=command,
                )
                results.append(error_result)

                if stop_on_error:
                    break

        return results

    async def _update_context(self):
        """Update execution context with current state for user mode debugging."""
        try:
            # For user mode debugging, set basic context without expensive commands
            self.context.current_process = "current"
            self.context.current_thread = 0

            # Only user mode debugging is supported

        except Exception as e:
            logger.warning(f"Failed to update context: {e}")

    @property
    def is_connected(self) -> bool:
        """Check if connected to WinDbg extension."""
        return self.comm_manager.get_connection_health().is_connected

    @property
    def needs_recovery(self) -> bool:
        """Check if the connection needs recovery."""
        return (
            self._consecutive_communication_errors >= self._max_consecutive_errors
            or not self.is_connected
        )

    def get_connection_health(self):
        """Get connection health status."""
        return self.comm_manager.get_connection_health()

    def suggest_recovery_action(self) -> Optional[str]:
        """Suggest recovery action based on current error state."""
        if self._consecutive_communication_errors >= self._max_consecutive_errors:
            # Try automatic recovery first
            logger.info(
                "[CMD_EXEC] Attempting automatic recovery due to consecutive errors..."
            )
            if self.comm_manager.force_reconnect():
                logger.info("[CMD_EXEC] Automatic recovery successful")
                self._consecutive_communication_errors = 0  # Reset counter
                return "Connection recovered automatically. You can now retry your commands."
            else:
                return (
                    "Multiple communication errors detected. Consider: "
                    "1) Check if WinDbg is still running, "
                    "2) Run '!vibedbg_status' in WinDbg to verify extension is loaded, "
                    "3) Try restarting the MCP server if needed"
                )

        health = self.get_connection_health()
        if not health.is_connected:
            return (
                "Connection lost to WinDbg extension. "
                "Run '!vibedbg_status' in WinDbg to check extension status."
            )

        if health.consecutive_failures > 3:
            return (
                "Multiple command failures detected. "
                "Target may not be in break state - try 'Ctrl+Break' in WinDbg to break execution."
            )

        return None
