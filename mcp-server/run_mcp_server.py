#!/usr/bin/env python3
"""
Wrapper script to run the VibeDbg MCP Server with proper environment setup.
"""

import os
import sys
import asyncio
from pathlib import Path

# Ensure we're in the right directory
script_dir = Path(__file__).parent
os.chdir(script_dir)

# Add current directory to Python path
sys.path.insert(0, str(script_dir))

# Set environment variables
os.environ["PYTHONPATH"] = str(script_dir)


def main():
    """Main entry point."""
    try:
        # Import and run the MCP server
        from src.server import main as server_main

        asyncio.run(server_main())
    except ImportError as e:
        print(f"Import error: {e}", file=sys.stderr)
        print(f"Python path: {sys.path}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error running MCP server: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
