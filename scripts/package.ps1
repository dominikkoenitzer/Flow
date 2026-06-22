# Package Script for FLOW (Windows)
# Builds a Release binary and produces the same unversioned artifacts the rolling
# "flow" release workflow does: the zip, the standalone exe, and a SHA-256
# checksum file in dist\.
#
# Usage: .\scripts\package.ps1
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildExe    = Join-Path $ProjectRoot "build\Release\FLOW.exe"
$DistRoot    = Join-Path $ProjectRoot "dist"
$DistName    = "FLOW-win64"
$DistDir     = Join-Path $DistRoot $DistName

Write-Host "== Building Release ==" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "build.ps1") Release
if (-not (Test-Path $BuildExe)) { throw "Build did not produce $BuildExe" }

Write-Host "== Verifying the binary is self-contained ==" -ForegroundColor Cyan
$imports = & objdump -p $BuildExe | Select-String "DLL Name"
$imports | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
if ($imports -match "libwinpthread|libstdc\+\+|libgcc|libgomp") {
    throw "Non-system DLL dependency found - build is NOT static. Aborting package."
}
if (-not (Select-String -Path $BuildExe -Pattern "requireAdministrator" -Quiet)) {
    throw "Manifest not embedded (no requireAdministrator). Aborting package."
}
Write-Host "  OK: self-contained + manifest embedded." -ForegroundColor Green

Write-Host "== Packaging $DistName ==" -ForegroundColor Cyan
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
New-Item -ItemType Directory -Path $DistDir | Out-Null
Copy-Item $BuildExe $DistDir
foreach ($f in @("README.md", "LICENSE", "LICENSE.txt", "LICENSE.md")) {
    $p = Join-Path $ProjectRoot $f
    if (Test-Path $p) { Copy-Item $p $DistDir }
}

$ZipPath = Join-Path $DistRoot "$DistName.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $DistDir -DestinationPath $ZipPath

$SumsPath = Join-Path $DistRoot "SHA256SUMS.txt"
$lines = foreach ($target in @($ZipPath, (Join-Path $DistDir "FLOW.exe"))) {
    $h = (Get-FileHash $target -Algorithm SHA256).Hash.ToLower()
    "$h  $((Resolve-Path $target -Relative))"
}
$lines | Set-Content -Path $SumsPath -Encoding ascii

Write-Host ""
Write-Host "== Artifacts in $DistRoot ==" -ForegroundColor Green
Write-Host "  $DistName.zip"
Write-Host "  $DistName\FLOW.exe"
Write-Host "  SHA256SUMS.txt"
Get-Content $SumsPath | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
