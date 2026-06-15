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
    Write-Host "  ✓ Found: $gccVersion" -ForegroundColor Green
} catch {
    Write-Host "  ✗ ERROR: g++ not found in PATH!" -ForegroundColor Red
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
Write-Host "  ✓ Build directory ready" -ForegroundColor Green

# Compile
Write-Host "[3/4] Compiling FLOW..." -ForegroundColor Yellow

# First compile the resource file
$ResourceFile = Join-Path $ProjectRoot "resource.rc"
$ResourceObj = Join-Path $BuildDir "resource.o"

if (Test-Path $ResourceFile) {
    Write-Host "  Compiling resources..." -ForegroundColor Gray
    & windres $ResourceFile -O coff -o $ResourceObj 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ⚠ Resource compilation failed, continuing without icon" -ForegroundColor Yellow
        $ResourceObj = $null
    } else {
        Write-Host "  ✓ Resources compiled" -ForegroundColor Green
    }
} else {
    $ResourceObj = $null
}

$SourceFiles = @(
    (Join-Path $SrcDir "main.cpp"),
    (Join-Path $SrcDir "FlowEngine.cpp")
)

# Add resource object if it was compiled successfully
if ($ResourceObj -and (Test-Path $ResourceObj)) {
    $SourceFiles += $ResourceObj
}

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
        Write-Host "  ✗ Compilation failed!" -ForegroundColor Red
        Write-Host $output -ForegroundColor Red
        exit 1
    }
    Write-Host "  ✓ Compilation successful" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Compilation error: $_" -ForegroundColor Red
    exit 1
}

# Verify output
Write-Host "[4/4] Verifying output..." -ForegroundColor Yellow
if (Test-Path $OutputExe) {
    $fileSize = (Get-Item $OutputExe).Length
    $fileSizeKB = [math]::Round($fileSize / 1KB, 2)
    Write-Host "  ✓ Executable created: $OutputExe" -ForegroundColor Green
    Write-Host "  Size: $fileSizeKB KB" -ForegroundColor Gray
} else {
    Write-Host "  ✗ Executable not found!" -ForegroundColor Red
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
