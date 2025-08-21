# VibeDbg - Project Overview for Claude

## Project Summary
VibeDbg is a conversational, LLM-powered AI assistant for WinDbg that transforms live debugging into a natural-language, interactive experience. It bridges the gap between traditional command-line debugging and modern AI assistance, making Windows debugging more accessible and intuitive.

## Architecture
The project consists of two main components:

### 1. WinDbg Extension (`ext/` directory)
- **Language**: C++ (Visual Studio 2022)
- **Purpose**: Native WinDbg extension for command execution
- **Key Features**:
  - Named pipe communication with MCP server
  - Session management and command handling
  - Comprehensive dual logging system (WinDbg UI + DebugView)
  - Simple command interface

### 2. MCP Server (`mcp-server/` directory)
- **Language**: Python 3.9+
- **Purpose**: Model Context Protocol server implementation
- **Package Manager**: uv (fast Python package manager)
- **Key Features**:
  - Tool integration for debugging operations
  - Communication layer between LLM and WinDbg
  - Multiple tool categories (debugging, analysis, performance, session, support)
  - Comprehensive exception handling with categorized error types
  - Robust connection pooling and retry mechanisms
  - Structured logging with severity levels

## Development Setup

### Building the WinDbg Extension
```powershell
cd ext
.\build.ps1 -Configuration Release
# Output: bin\x64\Release\VibeDbg.dll
```

### Setting up MCP Server
```bash
cd mcp-server
uv sync
uv sync --dev
uv run python -m vibedbg.mcp_server.server
```

## Testing Commands

### WinDbg Extension Testing
```cmd
# Load extension in WinDbg
.load "C:\github\VibeDbg\ext\bin\x64\Release\VibeDbg.dll"

# Available commands:
!vibedbg_status     # Show extension status
!vibedbg_connect    # Connect to MCP server
!vibedbg_execute    # Execute command through MCP
!vibedbg_test       # Run self-test
```

### MCP Server Testing
```bash
# Run tests
uv run pytest

# Code quality checks
uv run black . && uv run flake8 . && uv run mypy .
```

## Key Files to Understand

### Extension Core
- `ext/src/extension_main.cpp` - Main extension implementation
- `ext/src/core/command_executor.cpp` - Command execution logic
- `ext/src/communication/named_pipe_server.cpp` - Communication with MCP server

### MCP Server Core
- `mcp-server/src/server.py` - Main server entry point with comprehensive error handling
- `mcp-server/src/core/command_executor.py` - Command execution engine with routing and caching
- `mcp-server/src/core/communication.py` - Named pipe communication with connection pooling
- `mcp-server/src/core/exceptions.py` - Centralized exception handling framework
- `mcp-server/src/tools/core_tools.py` - Core debugging tools implementation
- `mcp-server/src/protocol/messages.py` - Message protocol definitions and validation
- `mcp-server/src/config.py` - Configuration management with environment support

## Build & Test Workflow
1. **Build Extension**: `cd ext && .\build.ps1 -Configuration Release`
2. **Setup MCP Server**: `cd mcp-server && uv sync && uv sync --dev`
3. **Test Extension**: Load in WinDbg and run `!vibedbg_test`
4. **Test MCP Server**: `uv run pytest`
5. **Code Quality**: `uv run black . && uv run flake8 . && uv run mypy .`

## Typical Usage Workflow
1. Start WinDbg with target process
2. Load VibeDbg extension: `.load VibeDbg.dll`
3. Connect to MCP server: `!vibedbg_connect`
4. Use natural language through LLM to debug interactively

## Exception Handling Architecture

The MCP server implements enterprise-grade exception handling:

### Exception Categories
- **Communication**: Named pipe and WinDbg extension communication errors
- **Validation**: Input validation and parameter checking failures  
- **Configuration**: Missing or invalid configuration issues
- **Execution**: Command execution and processing errors
- **Timeout**: Operation timeout and performance issues
- **Resource**: Memory and resource allocation problems
- **Security**: Permission and security violations

### Error Severity Levels
- **Critical**: System-breaking errors requiring immediate attention
- **High**: Serious errors that affect functionality
- **Medium**: Recoverable errors with potential workarounds
- **Low**: Minor issues and informational messages

### Key Features
- Automatic error categorization and severity assignment
- Centralized error handling with consistent logging
- Graceful degradation for non-critical failures
- Detailed error context and debugging information
- Retry mechanisms with smart failure detection

## Important Notes
- Extension is x64 platform only
- Requires Visual Studio 2022 for building C++ extension
- Uses uv package manager instead of pip for Python dependencies
- Comprehensive logging system for debugging the debugger
- MCP protocol enables seamless LLM integration
- Robust exception handling ensures reliable operation

## Current Status
Stable project with comprehensive error handling, connection pooling, and robust communication layer. Ready for production use with AI-powered debugging features.