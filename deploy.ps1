# ---------------------------------------------------------------------------
# deploy.ps1 - Copy the Qt runtime next to the executable (windeployqt)
#   powershell -ExecutionPolicy Bypass -File .\deploy.ps1
# Result: a self-contained folder in build\bin that can be copied anywhere.
#
# Qt / MinGW are auto-detected (see scripts\toolchain.ps1). Override with
# QT_DIR / MINGW_DIR environment variables if needed.
# This file must stay ASCII-only (Windows PowerShell 5.1 reads .ps1 as ANSI).
# ---------------------------------------------------------------------------
param(
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "scripts\toolchain.ps1")

$tc = Resolve-Toolchain -AutoInstall:$false
$QtDir = $tc.QtDir
$MingwDir = $tc.MingwDir
$env:Path = "$MingwDir\bin;$QtDir\bin;" + $env:Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $roots = @()
    # build.ps1 records the effective build root (ASCII junction) in .buildroot
    $marker = Join-Path $PSScriptRoot ".buildroot"
    if (Test-Path $marker) { $roots += ((Get-Content $marker -Raw).Trim()) }
    $roots += $PSScriptRoot
    foreach ($r in $roots) {
        if ($r -and (Test-Path (Join-Path $r "build\bin\ShanCi.exe"))) {
            $BuildDir = Join-Path $r "build"
            break
        }
    }
    if ([string]::IsNullOrWhiteSpace($BuildDir)) {
        $BuildDir = Join-Path $PSScriptRoot "build"
    }
}

$exe = Join-Path $BuildDir "bin\ShanCi.exe"
if (-not (Test-Path $exe)) {
    throw "Executable not found: $exe (run build.ps1 first)"
}

Write-Host "Deploying Qt runtime to $BuildDir\bin ..." -ForegroundColor Cyan
& "$QtDir\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw $exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Copy the bundled dictionaries so first-run import works out of the box
$dictSrc = Join-Path $PSScriptRoot "data\dict"
if (Test-Path $dictSrc) {
    $dictDst = Join-Path $BuildDir "bin\data\dict"
    New-Item -ItemType Directory -Force -Path $dictDst | Out-Null
    Copy-Item "$dictSrc\*.jsonl" $dictDst -Force
    Write-Host "Copied dictionaries to $dictDst" -ForegroundColor Cyan
}

Write-Host "Done. Portable folder: $BuildDir\bin" -ForegroundColor Green
