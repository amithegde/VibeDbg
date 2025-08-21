"""Tests for configuration module."""

import pytest
from src.config import (
    ServerConfig,
    OptimizationLevel,
    get_timeout_for_command,
    DEFAULT_PIPE_NAME,
    DEFAULT_BUFFER_SIZE,
)


class TestServerConfig:
    """Test ServerConfig functionality."""

    def test_default_config(self):
        """Test default configuration values."""
        config = ServerConfig()

        assert config.pipe_name == DEFAULT_PIPE_NAME
        assert config.buffer_size == DEFAULT_BUFFER_SIZE
        assert config.enable_command_validation is True
        assert config.optimization_level == OptimizationLevel.MODERATE
        assert config.max_connections == 10

    def test_optimization_levels(self):
        """Test optimization level enumeration."""
        assert OptimizationLevel.DISABLED.value == "disabled"
        assert OptimizationLevel.BASIC.value == "basic"
        assert OptimizationLevel.MODERATE.value == "moderate"
        assert OptimizationLevel.AGGRESSIVE.value == "aggressive"

    def test_timeout_calculation(self):
        """Test timeout calculation for different commands."""
        # Quick commands should have shorter timeouts
        assert get_timeout_for_command("version") == 10000
        assert get_timeout_for_command("help") == 10000

        # Analysis commands should have longer timeouts
        assert get_timeout_for_command("!analyze -v") > 30000
        assert get_timeout_for_command("!dump") > 30000

        # Default timeout for unknown commands
        default_timeout = get_timeout_for_command("unknown_command")
        assert default_timeout == 30000

    def test_from_environment(self):
        """Test configuration from environment variables."""
        import os

        # Set test environment variables
        os.environ["VIBEDBG_BUFFER_SIZE"] = "16384"
        os.environ["VIBEDBG_MAX_CONNECTIONS"] = "5"

        try:
            config = ServerConfig.from_environment()
            assert config.buffer_size == 16384
            assert config.max_connections == 5
        finally:
            # Clean up environment variables
            os.environ.pop("VIBEDBG_BUFFER_SIZE", None)
            os.environ.pop("VIBEDBG_MAX_CONNECTIONS", None)
