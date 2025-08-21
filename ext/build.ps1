#!/usr/bin/env pwsh

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [ValidateSet("x64")]
    [string]$Platform = "x64",
    [switch]$Clean,
    [switch]$Test
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Script configuration
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
$SolutionFile = Join-Path $ScriptDir "VibeDbg.sln"
$ProjectFile = Join-Path $ScriptDir "VibeDbg.vcxproj"
$BinDir = Join-Path $ScriptDir "bin\$Platform\$Configuration"
$ObjDir = Join-Path $ScriptDir "obj\$Platform\$Configuration"

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

function Test-MSBuild {
    Write-ColorOutput "Locating MSBuild..." "Cyan"
    
    $msbuildPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
    
    foreach ($path in $msbuildPaths) {
        if (Test-Path $path) {
            $script:MSBuildPath = $path
            Write-ColorOutput "Found MSBuild at: $path" "Green"
            return $true
        }
    }
    
    try {
        $msbuild = Get-Command MSBuild.exe -ErrorAction Stop
        $script:MSBuildPath = $msbuild.Source
        Write-ColorOutput "Found MSBuild in PATH: $($msbuild.Source)" "Green"
        return $true
    }
    catch {
        Write-ColorOutput "MSBuild not found. Please install Visual Studio Build Tools." "Red"
        return $false
    }
}

function Remove-BuildArtifacts {
    Write-ColorOutput "Cleaning build artifacts..." "Cyan"
    
    $dirsToClean = @($BinDir, $ObjDir)
    
    foreach ($dir in $dirsToClean) {
        if (Test-Path $dir) {
            Remove-Item $dir -Recurse -Force
            Write-ColorOutput "Cleaned $dir" "Green"
        }
    }
}

function Build-Extension {
    Write-ColorOutput "Building VibeDbg extension ($Configuration|$Platform)..." "Cyan"
    
    if (-not (Test-Path $SolutionFile)) {
        Write-ColorOutput "Solution file not found: $SolutionFile" "Red"
        exit 1
    }
    
    # Create output directories
    New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
    New-Item -ItemType Directory -Path $ObjDir -Force | Out-Null
    
    # Build arguments
    $buildArgs = @(
        $SolutionFile,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/p:OutDir=$BinDir\",
        "/p:IntDir=$ObjDir\",
        "/m",
        "/v:minimal",
        "/nologo"
    )
    
    if ($Clean) {
        $buildArgs += "/t:Clean"
    }
    
    # Execute MSBuild
    Write-ColorOutput "Executing: $MSBuildPath $($buildArgs -join ' ')" "Cyan"
    
    & $MSBuildPath @buildArgs
    
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "Build failed with exit code $LASTEXITCODE" "Red"
        exit 1
    }
    
    Write-ColorOutput "VibeDbg extension built successfully" "Green"
    Write-ColorOutput "Output: $BinDir" "Cyan"
}

function Test-BuildOutput {
    Write-ColorOutput "Verifying build output..." "Cyan"
    
    $expectedFiles = @("VibeDbg.dll", "VibeDbg.pdb")
    
    foreach ($file in $expectedFiles) {
        $filePath = Join-Path $BinDir $file
        if (Test-Path $filePath) {
            $size = (Get-Item $filePath).Length
            Write-ColorOutput "+ $file ($([math]::Round($size/1KB, 1)) KB)" "Green"
        }
        else {
            Write-ColorOutput "- $file not found" "Red"
            return $false
        }
    }
    
    return $true
}

function Show-ProjectInfo {
    Write-ColorOutput "Project Information:" "Cyan"
    Write-ColorOutput "===================" "Cyan"
    Write-ColorOutput "Solution: VibeDbg.sln" "White"
    Write-ColorOutput "Project: VibeDbg.vcxproj" "White"
    Write-ColorOutput "Configuration: $Configuration" "White"
    Write-ColorOutput "Platform: $Platform" "White"
    Write-ColorOutput "Output: $BinDir" "White"
    Write-ColorOutput ""
}

# Main execution
Write-ColorOutput "VibeDbg WinDbg Extension Build Script" "Cyan"
Write-ColorOutput "====================================" "Cyan"

Show-ProjectInfo

# Check MSBuild
if (-not (Test-MSBuild)) {
    exit 1
}

# Clean if requested
if ($Clean) {
    Remove-BuildArtifacts
}

# Build extension
Build-Extension

# Verify output
if (Test-BuildOutput) {
    Write-ColorOutput "Build verification completed successfully!" "Green"
    Write-ColorOutput "Extension ready for use with WinDbg:" "Green"
    Write-ColorOutput "  .load $BinDir\VibeDbg.dll" "Yellow"
}
else {
    Write-ColorOutput "Build verification failed!" "Red"
    exit 1
}

Write-ColorOutput "Build completed successfully!" "Green"