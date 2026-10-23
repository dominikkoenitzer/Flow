# Build Script for FLOW (Windows)
# Usage: .\scripts\build.ps1 [Release|Debug]

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Release', 'Debug')]
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " FLOW Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$OutputDir = Join-Path $BuildDir $BuildType
$SrcDir = Join-Path $ProjectRoot "src"
$IncludeDir = Join-Path $ProjectRoot "include"

Write-Host "Project Root: $ProjectRoot" -ForegroundColor Gray
Write-Host "Build Type:   $BuildType" -ForegroundColor Gray
Write-Host ""

# Check for g++ (MinGW)
Write-Host "[1/4] Checking for g++ compiler..." -ForegroundColor Yellow
try {
    $gccVersion = g++ --version 2>&1 | Select-Object -First 1
    Write-Host "  OK Found: $gccVersion" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: g++ not found in PATH!" -ForegroundColor Red
    Write-Host "  Please install MinGW-w64 and add it to PATH." -ForegroundColor Red
    exit 1
}

# Create build directory
Write-Host "[2/4] Creating build directory..." -ForegroundColor Yellow
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}
Write-Host "  OK Build directory ready" -ForegroundColor Green

# Compile
Write-Host "[3/4] Compiling FLOW..." -ForegroundColor Yellow

# First compile the resource file. It embeds the icon and the manifest that
# asks for Administrator rights, so the build stops if windres fails.
$ResourceFile = Join-Path $ProjectRoot "resource.rc"
$ResourceObj = Join-Path $BuildDir "resource.o"

Write-Host "  Compiling resources..." -ForegroundColor Gray
try {
    $windresOutput = & windres $ResourceFile -O coff -o $ResourceObj 2>&1
    $windresFailed = $LASTEXITCODE -ne 0
} catch {
    $windresOutput = $_
    $windresFailed = $true
}
if ($windresFailed) {
    Write-Host "  ERROR: Resource compilation failed!" -ForegroundColor Red
    Write-Host "  FLOW.exe needs the manifest from resource.rc to ask for Administrator rights." -ForegroundColor Red
    Write-Host "  Check that windres is in PATH (it ships with MinGW-w64 binutils)." -ForegroundColor Red
    Write-Host $windresOutput -ForegroundColor Red
    exit 1
}
Write-Host "  OK Resources compiled" -ForegroundColor Green

# Every .cpp under src/, so a new translation unit builds without editing this
# script. Sorted for a reproducible command line.
$SourceFiles = @(
    Get-ChildItem -Path $SrcDir -Filter *.cpp -Recurse |
        Sort-Object FullName |
        Select-Object -ExpandProperty FullName
)
$SourceFiles += $ResourceObj

$OutputExe = Join-Path $OutputDir "FLOW.exe"

$CompileArgs = @(
    "-std=c++17",
    "-I$IncludeDir",
    "-o", $OutputExe
) + $SourceFiles + @(
    "-mwindows",
    "-luser32",
    "-lgdi32",
    "-lcomctl32",
    "-lgdiplus",
    "-lshell32",
    "-lole32",      # the shell link object behind Create Play Shortcut
    "-luuid",
    # -static bundles libwinpthread/libstdc++/libgcc INTO the exe so it runs on
    # machines without MinGW. Only system DLLs (user32, gdi32, ...) stay external.
    "-static",
    "-static-libgcc",
    "-static-libstdc++"
)

if ($BuildType -eq "Release") {
    $CompileArgs += @("-O3", "-DNDEBUG")
    Write-Host "  Building with optimizations (-O3)..." -ForegroundColor Gray
} else {
    $CompileArgs += @("-g", "-O0", "-DDEBUG")
    Write-Host "  Building with debug symbols (-g)..." -ForegroundColor Gray
}

$CompileCommand = "g++ $($CompileArgs -join ' ')"
Write-Host "  Command: $CompileCommand" -ForegroundColor DarkGray

try {
    $output = & g++ $CompileArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERROR: Compilation failed!" -ForegroundColor Red
        Write-Host $output -ForegroundColor Red
        exit 1
    }
    Write-Host "  OK Compilation successful" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: Compilation error: $_" -ForegroundColor Red
    exit 1
}

# Verify output
Write-Host "[4/4] Verifying output..." -ForegroundColor Yellow
if (Test-Path $OutputExe) {
    $fileSize = (Get-Item $OutputExe).Length
    $fileSizeKB = [math]::Round($fileSize / 1KB, 2)
    Write-Host "  OK Executable created: $OutputExe" -ForegroundColor Green
    Write-Host "  Size: $fileSizeKB KB" -ForegroundColor Gray
} else {
    Write-Host "  ERROR: Executable not found!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " Build Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "To run FLOW:" -ForegroundColor Cyan
Write-Host "  $OutputExe" -ForegroundColor White
Write-Host ""
Write-Host "Note: Run as Administrator for hook functionality" -ForegroundColor Yellow
Write-Host ""
