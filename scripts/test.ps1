# Test Script for FLOW (Windows)
# Usage: .\scripts\test.ps1 [-Filter <pattern>]
#
# Builds and runs the doctest suite over FlowEngine. The GUI is not linked in,
# so this needs neither a window nor administrator rights.

param(
    [Parameter(Mandatory=$false)]
    [string]$Filter = ''
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " FLOW Test Suite" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir    = Join-Path $ProjectRoot "build"
$IncludeDir  = Join-Path $ProjectRoot "include"
$TestsDir    = Join-Path $ProjectRoot "tests"
$SrcDir      = Join-Path $ProjectRoot "src"
$OutputExe   = Join-Path $BuildDir "FLOW_Tests.exe"

Write-Host "[1/3] Checking for g++ compiler..." -ForegroundColor Yellow
try {
    $gccVersion = g++ --version 2>&1 | Select-Object -First 1
    Write-Host "  OK Found: $gccVersion" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: g++ not found in PATH!" -ForegroundColor Red
    Write-Host "  Please install MinGW-w64 and add it to PATH." -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Write-Host "[2/3] Compiling tests..." -ForegroundColor Yellow

# Every test .cpp plus the engine. main.cpp is deliberately excluded -- it owns
# WinMain and the whole GUI, neither of which the engine tests need.
$Sources = @(
    Get-ChildItem -Path $TestsDir -Filter *.cpp |
        Sort-Object FullName |
        Select-Object -ExpandProperty FullName
) + (Join-Path $SrcDir "FlowEngine.cpp")

$CompileArgs = @(
    "-std=c++17",
    "-O2",
    "-Wall",
    "-Wextra",
    "-I$IncludeDir",
    "-I$TestsDir",
    "-o", $OutputExe
) + $Sources + @(
    "-luser32",
    "-static",
    "-static-libgcc",
    "-static-libstdc++"
)

$output = & g++ $CompileArgs 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Compilation failed!" -ForegroundColor Red
    Write-Host $output -ForegroundColor Red
    exit 1
}
Write-Host "  OK Compiled" -ForegroundColor Green

Write-Host "[3/3] Running tests..." -ForegroundColor Yellow
Write-Host ""

if ($Filter) {
    & $OutputExe --test-case="$Filter"
} else {
    & $OutputExe
}
$testExit = $LASTEXITCODE

Write-Host ""
if ($testExit -eq 0) {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " All tests passed" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
} else {
    Write-Host "========================================" -ForegroundColor Red
    Write-Host " Tests failed" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
}

exit $testExit
