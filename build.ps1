# ---------------------------------------------------------------------------
# build.ps1 - Build ShanCi (Qt 6 + MinGW-w64 + Ninja)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\build.ps1            # build
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean     # clean then build
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Tests     # build and run tests
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Run       # build and launch
#
# Toolchain:
#   Qt / MinGW / Ninja are auto-detected (C:\Qt, %USERPROFILE%\Qt,
#   %LOCALAPPDATA%\Qt, qmake/g++/ninja on PATH).
#   If anything is missing the script installs it automatically with aqtinstall
#   (needs Python 3 + internet). Use -NoAutoInstall to disable that.
#
#   Manual overrides:
#     -QtDir <path>  -MingwDir <path>  -NinjaDir <path>  -QtVersion <ver>
#   or environment variables QT_DIR / MINGW_DIR / NINJA_DIR.
#
# This file must stay ASCII-only (Windows PowerShell 5.1 reads .ps1 as ANSI).
# ---------------------------------------------------------------------------
param(
    [switch]$Clean,
    [switch]$Tests,
    [switch]$Run,
    [switch]$NoAutoInstall,
    [string]$QtVersion = "6.8.3",
    [string]$QtDir = "",
    [string]$MingwDir = "",
    [string]$NinjaDir = "",
    [string]$Junction = "C:\shanci"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "scripts\toolchain.ps1")

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found on PATH. Install CMake 3.21+ (https://cmake.org/download/) and re-run."
}

# --- verify the project is complete (catches partially copied folders) ---
$requiredFiles = @(
    "CMakeLists.txt",
    "tests\CMakeLists.txt",
    "third_party\sqlite\sqlite3.c",
    "third_party\sqlite\sqlite3.h",
    "src\main.cpp",
    "src\core\AppContext.cpp",
    "src\core\AppContext.h",
    "src\domain\Entities.h",
    "src\data\Database.cpp",
    "src\service\Services.h",
    "src\generator\QuestionStrategy.cpp",
    "src\ui\MainWindow.cpp",
    "tools\package.py",
    "tools\bootstrapper\Bootstrapper.c",
    "scripts\toolchain.ps1"
)
$missing = @()
foreach ($rel in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $rel))) { $missing += $rel }
}
if ($missing.Count -gt 0) {
    $nl = [Environment]::NewLine
    throw ("Project is incomplete - missing " + $missing.Count + " file(s):" + $nl +
           "  " + ($missing -join ($nl + "  ")) + $nl +
           "Copy the whole project folder again (or extract dist\ShanCi-source.zip).")
}

$dictMissing = @()
foreach ($rel in @("data\dict\CET4.jsonl", "data\dict\CET6.jsonl")) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $rel))) { $dictMissing += $rel }
}
if ($dictMissing.Count -gt 0) {
    Write-Host ("Warning: built-in dictionaries missing (" + ($dictMissing -join ", ") +
                "). The app will start with an empty dictionary.") -ForegroundColor Yellow
}

# --- explicit overrides take precedence over auto-detection ---
if ($QtDir)    { $env:QT_DIR = $QtDir }
if ($MingwDir) { $env:MINGW_DIR = $MingwDir }
if ($NinjaDir) { $env:NINJA_DIR = $NinjaDir }

$autoInstall = -not $NoAutoInstall
$tc = Resolve-Toolchain -QtVersion $QtVersion -AutoInstall:$autoInstall
$QtDir = $tc.QtDir
$MingwDir = $tc.MingwDir
$NinjaDir = $tc.NinjaDir

Write-Host "Using Qt      : $QtDir" -ForegroundColor DarkGray
Write-Host "Using MinGW   : $MingwDir" -ForegroundColor DarkGray
if ($NinjaDir) { Write-Host "Using Ninja   : $NinjaDir" -ForegroundColor DarkGray }

# --- resolve the source root used for building (create ASCII junction if needed) ---
$RealRoot = (Resolve-Path $PSScriptRoot).Path
$SrcRoot  = $RealRoot
$isAscii  = $RealRoot -match '^[\x00-\x7F]+$'

if (-not $isAscii) {
    $chosen = $null
    if (Test-Path -LiteralPath $Junction) {
        $item = Get-Item $Junction -Force
        $isLink = [bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)
        $target = @($item.Target)[0]
        $resolved = if ($target) { (Resolve-Path $target -ErrorAction SilentlyContinue).Path } else { $null }
        if ($isLink -and $resolved -and ($resolved -eq $RealRoot)) {
            $chosen = $Junction
        } elseif ($isLink) {
            Write-Host "Removing stale junction: $Junction -> $target" -ForegroundColor Yellow
            cmd /c "rmdir `"$Junction`"" | Out-Null
        } else {
            Write-Host "Path $Junction exists and is not a junction; trying another location." -ForegroundColor Yellow
        }
    }

    if (-not $chosen) {
        $candidates = @($Junction)
        if ($env:LOCALAPPDATA) { $candidates += (Join-Path $env:LOCALAPPDATA "ShanCi-build") }
        foreach ($j in $candidates) {
            if (Test-Path -LiteralPath $j) { continue }
            Write-Host "Creating ASCII junction: $j -> $RealRoot" -ForegroundColor Yellow
            cmd /c "mklink /J `"$j`" `"$RealRoot`"" | Out-Null
            if (Test-Path -LiteralPath $j) { $chosen = $j; break }
        }
        if (-not $chosen) {
            throw ("Cannot create an ASCII directory junction (needed because the project path" +
                   " contains non-ASCII characters). Move the project to an ASCII path such as" +
                   " C:\Dev\ShanCi and re-run.")
        }
    }

    Write-Host ("Note: the project path contains non-ASCII characters, so a temporary ASCII" +
                " link is used for the compiler:") -ForegroundColor DarkGray
    Write-Host ("      $chosen  ->  $RealRoot") -ForegroundColor DarkGray
    Write-Host ("      All source files still live inside the project folder; the link is only a path alias.") -ForegroundColor DarkGray

    $SrcRoot = $chosen
    # 记录实际构建根目录，供 deploy.ps1 / package.py 复用
    Set-Content -Path (Join-Path $RealRoot ".buildroot") -Value $SrcRoot -Encoding ASCII
} else {
    $marker = Join-Path $RealRoot ".buildroot"
    if (Test-Path $marker) { Remove-Item $marker -Force }
}

$BuildDir = Join-Path $SrcRoot "build"

# --- pick a generator: prefer Ninja, fall back to MinGW Makefiles on ASCII paths ---
if ($NinjaDir) {
    $Generator = "Ninja"
    $generatorArgs = @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$NinjaDir\ninja.exe")
    $env:Path = "$MingwDir\bin;$QtDir\bin;$NinjaDir;" + $env:Path
} else {
    if (-not $isAscii) {
        throw ("Ninja is required when the project path contains non-ASCII characters" +
               " (Qt's moc cannot handle them). Install Ninja or set NINJA_DIR.")
    }
    Write-Host "Ninja not found - falling back to MinGW Makefiles." -ForegroundColor Yellow
    $Generator = "MinGW Makefiles"
    $generatorArgs = @("-G", "MinGW Makefiles", "-DCMAKE_MAKE_PROGRAM=$MingwDir\bin\mingw32-make.exe")
    $env:Path = "$MingwDir\bin;$QtDir\bin;" + $env:Path
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build dir $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# --- re-configure automatically when the source root or generator changed ---
$cache = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cache) {
    $wipe = $false
    $homeLine = Select-String -Path $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$' -ErrorAction SilentlyContinue
    if ($homeLine -and ($homeLine.Matches[0].Groups[1].Value -ne $SrcRoot)) { $wipe = $true }
    $genLine = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' -ErrorAction SilentlyContinue
    if ($genLine -and ($genLine.Matches[0].Groups[1].Value -ne $Generator)) { $wipe = $true }
    if ($wipe) {
        Write-Host "Build configuration changed, reconfiguring" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
}

$testFlag = if ($Tests) { "ON" } else { "OFF" }

Write-Host "Configuring CMake (source: $SrcRoot) ..." -ForegroundColor Cyan
cmake -S $SrcRoot -B $BuildDir @generatorArgs `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_C_COMPILER="$MingwDir\bin\gcc.exe" `
    -DCMAKE_CXX_COMPILER="$MingwDir\bin\g++.exe" `
    -DVOCAB_BUILD_TESTS=$testFlag
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "Building ..." -ForegroundColor Cyan
cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

if ($Tests) {
    Write-Host "Running tests ..." -ForegroundColor Cyan
    Push-Location $BuildDir
    ctest --output-on-failure
    Pop-Location
}

Write-Host "Done. Executable: $RealRoot\build\bin" -ForegroundColor Green

if ($Run) {
    Write-Host "Launching ..." -ForegroundColor Cyan
    & (Join-Path $BuildDir "bin\ShanCi.exe")
}
