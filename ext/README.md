# VibeDbg WinDbg Extension

A WinDbg extension that provides AI-assisted debugging capabilities through Model Context Protocol (MCP) integration.

> **Note**: This extension is located in the `ext/` directory to clearly identify it as a WinDbg extension component.

## 🚀 Features

- **WinDbg Integration**: Native WinDbg extension with proper lifecycle management
- **MCP Protocol Support**: Ready for integration with AI models via MCP server
- **Comprehensive Logging**: Dual logging system with WinDbg UI and DebugView support
- **Simple Commands**: Basic commands for status, connection, and execution
- **Production Ready**: Clean build with Debug and Release configurations

## 📁 Project Structure

```
ext/
├── src/                          # Source files
│   ├── dllmain.cpp              # DLL entry point
│   ├── extension_main.cpp       # Main extension implementation
│   ├── pch.h/cpp               # Precompiled headers
│   ├── framework.h              # Framework includes
│   ├── VibeDbg.def              # Module definition file
│   ├── core/                    # Core functionality
│   ├── communication/           # Communication components
│   └── utils/                   # Utility functions
├── inc/                         # Header files (.h)
│   ├── command_executor.h       # Command execution interface
│   ├── constants.h              # Project constants
│   ├── error_handling.h         # Error handling utilities
│   ├── extension.h              # Main extension interface
│   ├── json.h                   # JSON utilities
│   ├── logging.h                # Comprehensive logging system
│   ├── message_protocol.h       # Message protocol definitions
│   ├── named_pipe_server.h      # Named pipe server interface
│   └── session_manager.h        # Session management interface
├── bin/                         # Build output directory
├── VibeDbg.vcxproj              # Visual Studio project file
├── VibeDbg.sln                  # Visual Studio solution file
├── build.ps1                   # PowerShell build script
└── test_extension.ps1          # Testing instructions
```

## 🛠️ Building

### Requirements
- Visual Studio 2022 (Enterprise/Professional/Community)
- Windows 10/11 SDK
- WinDbg SDK (usually included with Windows SDK)

### Quick Build
```powershell
# Build Release version
.\build.ps1 -Configuration Release

# Build Debug version  
.\build.ps1 -Configuration Debug

# Clean build
.\build.ps1 -Configuration Release -Clean
```

### Manual Build
```powershell
# Using MSBuild directly
msbuild VibeDbg.sln /p:Configuration=Release /p:Platform=x64
```

## 📦 Installation

1. Build the extension (see Building section)
2. The output will be in `bin\x64\Release\VibeDbg.dll`
3. Load in WinDbg: `.load "path\to\VibeDbg.dll"`

## 🔧 Usage

### Available Commands

| Command | Description | Usage |
|---------|-------------|-------|
| `!vibedbg_status` | Show extension status and version | `!vibedbg_status` |
| `!vibedbg_connect` | Connect to MCP server (placeholder) | `!vibedbg_connect` |
| `!vibedbg_execute` | Execute command through MCP (placeholder) | `!vibedbg_execute <command>` |
| `!vibedbg_test` | Run self-test and demonstrate logging | `!vibedbg_test` |

### Logging System

The VibeDbg extension features a comprehensive dual logging system:

#### WinDbg UI Logging
- **Purpose**: User-visible status messages in WinDbg UI
- **Usage**: `LOG_WINDBG(context, message)`
- **Example**: Shows connection status, command results, and user feedback

#### DebugView Logging
- **Purpose**: Detailed internal logs captured by DebugView
- **Usage**: `LOG_INFO(context, message)`, `LOG_ERROR(context, message)`, etc.
- **Format**: `[Timestamp] [Level] [Component] [Context] Message | Details`
- **Example**: `[2024-01-15 10:30:45.123] [INFO] [VibeDbg] [Connect] Extension initialized successfully`

#### Log Levels
- **TRACE**: Detailed execution flow
- **DEBUG**: Debugging information
- **INFO**: General information
- **WARNING**: Warning messages
- **ERROR**: Error conditions
- **FATAL**: Critical errors

#### Viewing DebugView Logs
1. Download [DebugView](https://docs.microsoft.com/en-us/sysinternals/downloads/debugview) from Microsoft Sysinternals
2. Run DebugView as Administrator
3. Filter for "VibeDbg" to see extension logs
4. Logs include timestamps, context, and detailed information

#### Example Log Output
```
[2024-01-15 10:30:45.123] [INFO] [VibeDbg] [Extension] DebugExtensionInitialize called
[2024-01-15 10:30:45.124] [INFO] [VibeDbg] [Extension] DebugExtensionInitialize completed successfully
[2024-01-15 10:30:45.125] [INFO] [VibeDbg] [Connect] Starting extension initialization
[2024-01-15 10:30:45.126] [INFO] [VibeDbg] [Extension] Initializing debugger interfaces...
[2024-01-15 10:30:45.127] [INFO] [VibeDbg] [Extension] Debugger interfaces initialized
[2024-01-15 10:30:45.128] [INFO] [VibeDbg] [Extension] Core components initialized
[2024-01-15 10:30:45.129] [INFO] [VibeDbg] [Extension] Communication initialized
[2024-01-15 10:30:45.130] [INFO] [VibeDbg] [Connect] Extension initialized successfully
```

### Example WinDbg Session
```
0:000> .load "C:\path\to\VibeDbg.dll"
0:000> !vibedbg_status
VibeDbg Extension Status:
Version: VibeDbg v1.0.0
Extension: VibeDbg WinDbg Extension
Status: Active

0:000> !vibedbg_execute "analyze -v"
VibeDbg: Executing command: analyze -v
VibeDbg: Command execution functionality will be implemented
```

## 🏗️ Architecture

### Core Components
- **Extension Lifecycle**: Proper WinDbg extension initialization/cleanup
- **Debug Interface Management**: IDebugClient, IDebugControl, IDebugSymbols
- **Command Handlers**: Extensible command processing framework
- **MCP Ready**: Infrastructure ready for MCP server integration

### Design Principles
- **Simplicity**: C++17 standard with minimal dependencies
- **Robustness**: Proper error handling and resource management
- **Extensibility**: Easy to add new commands and features
- **Performance**: Lightweight with minimal overhead

## 🔍 Development

### Adding New Commands
1. Add function declaration in `extension_main_simple.cpp`
2. Implement the command handler following the pattern:
   ```cpp
   extern "C" HRESULT CALLBACK your_command(PDEBUG_CLIENT Client, PCSTR Args) {
       // Implementation
       return S_OK;
   }
   ```
3. Add export to `VibeDbg.def`
4. Rebuild the extension

### Debug Information
- Debug builds include full symbols in `.pdb` files
- Use `dprintf()` for output in WinDbg console
- Error handling via HRESULT return codes

## 📋 Technical Notes

### Compiler Settings
- **Language Standard**: C++17 (compatible with MSBuild)
- **Platform**: x64 only (WinDbg extension requirement)
- **Warnings**: Level 3, some unreferenced parameters suppressed

### Dependencies
- **dbgeng.lib**: WinDbg Engine API
- **dbghelp.lib**: Debugging helper functions
- **Standard Windows libraries**: kernel32, user32

### Known Limitations
- MCP server integration is stubbed for future implementation
- Limited to basic commands in current version
- x64 platform only

## 🚧 Future Enhancements

- [ ] Complete MCP server implementation
- [ ] Named pipe communication for AI integration
- [ ] Advanced command processing
- [ ] Session state management
- [ ] Configuration management

## 📝 License

This project is part of the VibeDbg debugging assistance system.

## 🤝 Contributing

1. Follow the existing code style and patterns
2. Test all changes with both Debug and Release builds
3. Update documentation for new features
4. Ensure all WinDbg commands work correctly

---

**Ready to load and test in WinDbg!** 🎯