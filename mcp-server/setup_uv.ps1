# VibeDbg Python Project - UV Setup Script
# This script helps set up the Python project using uv instead of pip

Write-Host "VibeDbg Python Project - UV Setup" -ForegroundColor Green
Write-Host "=================================" -ForegroundColor Green

# Check if uv is installed
try {
    $uvVersion = uv --version
    Write-Host "✓ UV is installed: $uvVersion" -ForegroundColor Green
} catch {
    Write-Host "✗ UV is not installed. Please install uv first:" -ForegroundColor Red
    Write-Host "  Visit: https://docs.astral.sh/uv/getting-started/installation/" -ForegroundColor Yellow
    Write-Host "  Or run: pip install uv" -ForegroundColor Yellow
    exit 1
}

# Check if we're in the right directory
if (-not (Test-Path "pyproject.toml")) {
    Write-Host "✗ pyproject.toml not found. Please run this script from the mcp-server directory." -ForegroundColor Red
    exit 1
}

Write-Host "`nSetting up virtual environment and installing dependencies..." -ForegroundColor Cyan

# Create virtual environment and install dependencies
try {
    uv sync
    Write-Host "✓ Dependencies installed successfully!" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to install dependencies. Error: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`nInstalling development dependencies..." -ForegroundColor Cyan

# Install development dependencies
try {
    uv sync --dev
    Write-Host "✓ Development dependencies installed successfully!" -ForegroundColor Green
} catch {
    Write-Host "✗ Failed to install development dependencies. Error: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`nUV Setup Complete!" -ForegroundColor Green
Write-Host "==================" -ForegroundColor Green
Write-Host "You can now use the following commands:" -ForegroundColor Cyan
Write-Host "  uv run python -m vibedbg.mcp_server.server    # Run the MCP server" -ForegroundColor White
Write-Host "  uv run pytest                                # Run tests" -ForegroundColor White
Write-Host "  uv run black .                               # Format code" -ForegroundColor White
Write-Host "  uv run flake8 .                              # Lint code" -ForegroundColor White
Write-Host "  uv run mypy .                                # Type check" -ForegroundColor White
Write-Host "`nNote: This project uses uv instead of pip for dependency management." -ForegroundColor Yellow
