# VibeDbg Python MCP Server

This directory contains the Python implementation of the VibeDbg MCP (Model Context Protocol) Server for LLM-driven Windows debugging.

## Prerequisites

- Python 3.9 or higher
- [uv](https://docs.astral.sh/uv/getting-started/installation/) - Fast Python package manager
- Windows (for WinDbg integration)

## Quick Start

1. **Install uv** (if not already installed):
   ```bash
   pip install uv
   ```

2. **Set up the project**:
   ```bash
   cd mcp-server
   ./setup_uv.ps1  # On Windows PowerShell
   # or manually:
   uv sync
   uv sync --dev
   ```

3. **Run the MCP server**:
   ```bash
   uv run python -m vibedbg.mcp_server.server
   ```

## Project Structure

```
mcp-server/
├── pyproject.toml          # Project configuration (uv/hatchling)
├── setup_uv.ps1           # UV setup script
├── src/
│   └── vibedbg/
│       ├── mcp_server/    # MCP server implementation
│       │   ├── server.py  # Main server entry point
│       │   ├── config.py  # Server configuration
│       │   ├── core/      # Core functionality
│       │   └── tools/     # MCP tools
│       ├── protocol/      # MCP protocol implementation
│       └── testing/       # Testing utilities
└── tests/                 # Test suite
```

## Development

### Using uv

This project uses [uv](https://docs.astral.sh/uv/) for dependency management instead of pip. Key commands:

```bash
# Install dependencies
uv sync

# Install development dependencies
uv sync --dev

# Run commands in the virtual environment
uv run python -m vibedbg.mcp_server.server
uv run pytest
uv run black .
uv run flake8 .
uv run mypy .

# Add a new dependency
uv add package-name

# Add a development dependency
uv add --dev package-name

# Update dependencies
uv lock --upgrade
```

### Available Tools

The MCP server provides the following tool categories:

- **Debugging Tools**: Execute WinDbg commands, manage breakpoints, step through code
- **Analysis Tools**: Analyze crash dumps, memory leaks, performance issues
- **Performance Tools**: Monitor CPU, memory, thread activity
- **Session Tools**: Manage debugging sessions, save/load state
- **Support Tools**: Help, symbol search, system information

### Testing

```bash
# Run all tests
uv run pytest

# Run tests with coverage
uv run pytest --cov=vibedbg

# Run specific test categories
uv run pytest tests/unit/
uv run pytest tests/integration/
```

### Code Quality

```bash
# Format code
uv run black .

# Lint code
uv run flake8 .

# Type checking
uv run mypy .

# Run all quality checks
uv run black . && uv run flake8 . && uv run mypy .
```

## Configuration

The server can be configured through the `ServerConfig` class in `src/vibedbg/mcp_server/config.py`. Key configuration options:

- `debugging_mode`: Debugging strategy (basic, advanced, expert)
- `optimization_level`: Performance optimization level
- `timeout`: Command execution timeout
- `max_connections`: Maximum concurrent connections

## MCP Protocol

This server implements the Model Context Protocol (MCP) specification, providing:

- **Tools**: Execute WinDbg commands and debugging operations
- **Resources**: Access to debugging information and system data
- **Prompts**: Predefined debugging scenarios and workflows

## Dependencies

### Core Dependencies
- `mcp>=1.0.0` - Official MCP SDK
- `pywin32>=306` - Windows API access
- `pydantic>=2.0.0` - Data validation
- `asyncio>=3.4.3` - Asynchronous I/O
- `typing-extensions>=4.0.0` - Type hints

### Development Dependencies
- `pytest>=7.0.0` - Testing framework
- `pytest-asyncio>=0.21.0` - Async testing support
- `black>=23.0.0` - Code formatting
- `flake8>=6.0.0` - Code linting
- `mypy>=1.0.0` - Type checking
- `pytest-cov>=4.0.0` - Coverage reporting

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests and quality checks
5. Submit a pull request

## License

This project is part of the VibeDbg project. See the main project license for details.
