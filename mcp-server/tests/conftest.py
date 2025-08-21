"""Shared test fixtures and configuration."""

import pytest
import sys
from pathlib import Path

# Add src directory to path for imports
src_path = Path(__file__).parent.parent / "src"
sys.path.insert(0, str(src_path))


@pytest.fixture(scope="session")
def event_loop():
    """Create an instance of the default event loop for the test session."""
    import asyncio

    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()


@pytest.fixture
def mock_windbg_response():
    """Mock WinDbg command response."""
    return {"success": True, "output": "Mock WinDbg output", "execution_time_ms": 100}
