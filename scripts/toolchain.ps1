# ---------------------------------------------------------------------------
# toolchain.ps1 - Locate (and optionally install) Qt 6 / MinGW-w64 / Ninja.
#
# Dot-source this file, then:
#     $tc = Resolve-Toolchain -QtVersion "6.8.3" -AutoInstall
#     $tc.QtDir / $tc.MingwDir / $tc.NinjaDir
#
# Overrides (environment variables):
#     QT_DIR              e.g. C:\Qt\6.8.3\mingw_64
#     MINGW_DIR           e.g. C:\Qt\Tools\mingw1310_64
#     NINJA_DIR           directory containing ninja.exe
#     QT_INSTALL_ROOT     where aqtinstall puts a downloaded Qt (default C:\Qt)
#
# This file must stay ASCII-only (Windows PowerShell 5.1 reads .ps1 as ANSI
# unless a BOM is present).
# ---------------------------------------------------------------------------

function Test-QtDir {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    return (Test-Path (Join-Path $Path "lib\cmake\Qt6\Qt6Config.cmake"))
}

function Get-QtCandidateDirs {
    $list = New-Object System.Collections.ArrayList
    if ($env:QT_DIR) { [void]$list.Add($env:QT_DIR) }

    $roots = @("C:\Qt")
    if ($env:USERPROFILE) { $roots += (Join-Path $env:USERPROFILE "Qt") }
    if ($env:LOCALAPPDATA) { $roots += (Join-Path $env:LOCALAPPDATA "Qt") }

    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $vers = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^[0-9]+\.[0-9]+' }
        $vers = $vers | Sort-Object -Property @{ Expression = {
                    $p = $_.Name -split '\.'
                    ([int]$p[0]) * 1000 + ([int]$p[1])
                } } -Descending
        foreach ($v in $vers) {
            [void]$list.Add((Join-Path $v.FullName "mingw_64"))
            [void]$list.Add((Join-Path $v.FullName "msvc2022_64"))
            [void]$list.Add((Join-Path $v.FullName "msvc2019_64"))
        }
    }

    $q = Get-Command qmake6 -ErrorAction SilentlyContinue
    if (-not $q) { $q = Get-Command qmake -ErrorAction SilentlyContinue }
    if ($q) { [void]$list.Add((Split-Path (Split-Path $q.Source -Parent) -Parent)) }

    return $list
}

function Find-Qt {
    foreach ($c in (Get-QtCandidateDirs)) {
        if (Test-QtDir $c) { return (Resolve-Path -LiteralPath $c).Path }
    }
    return $null
}

function Find-Mingw {
    param([string]$QtDir)
    $list = New-Object System.Collections.ArrayList
    if ($env:MINGW_DIR) { [void]$list.Add($env:MINGW_DIR) }

    if ($QtDir) {
        $qtRoot = Split-Path (Split-Path $QtDir -Parent) -Parent
        $tools = Join-Path $qtRoot "Tools"
        if (Test-Path -LiteralPath $tools) {
            Get-ChildItem -LiteralPath $tools -Directory -Filter "mingw*" -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { [void]$list.Add($_.FullName) }
        }
    }
    [void]$list.Add("C:\mingw64")
    [void]$list.Add("D:\mingw64")

    $g = Get-Command g++ -ErrorAction SilentlyContinue
    if ($g) { [void]$list.Add((Split-Path (Split-Path $g.Source -Parent) -Parent)) }

    foreach ($c in $list) {
        if ($c -and (Test-Path -LiteralPath (Join-Path $c "bin\g++.exe"))) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    return $null
}

function Find-Ninja {
    param([string]$QtDir)
    $list = New-Object System.Collections.ArrayList
    if ($env:NINJA_DIR) { [void]$list.Add($env:NINJA_DIR) }
    if ($QtDir) {
        $qtRoot = Split-Path (Split-Path $QtDir -Parent) -Parent
        [void]$list.Add((Join-Path $qtRoot "Tools\Ninja"))
    }
    [void]$list.Add("C:\Qt\Tools\Ninja")
    $n = Get-Command ninja -ErrorAction SilentlyContinue
    if ($n) { [void]$list.Add((Split-Path $n.Source -Parent)) }

    foreach ($c in $list) {
        if ($c -and (Test-Path -LiteralPath (Join-Path $c "ninja.exe"))) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }
    return $null
}

function Get-PythonCommand {
    foreach ($name in @("python", "py")) {
        $c = Get-Command $name -ErrorAction SilentlyContinue
        if ($c) { return $c.Source }
    }
    return $null
}

function Get-ToolchainInstallRoot {
    if ($env:QT_INSTALL_ROOT) { return $env:QT_INSTALL_ROOT }
    $preferred = "C:\Qt"
    try {
        if (-not (Test-Path -LiteralPath $preferred)) {
            New-Item -ItemType Directory -Path $preferred -ErrorAction Stop | Out-Null
        }
        return $preferred
    } catch { }
    $fallback = Join-Path $env:LOCALAPPDATA "Qt"
    if (-not (Test-Path -LiteralPath $fallback)) {
        New-Item -ItemType Directory -Path $fallback -Force | Out-Null
    }
    return $fallback
}

function Install-Toolchain {
    param([string]$QtVersion = "6.8.3")

    $python = Get-PythonCommand
    if (-not $python) {
        throw ("Qt 6 was not found and Python is not installed." + [Environment]::NewLine +
               "Install Python 3 (https://www.python.org/downloads/) and re-run, or install Qt " +
               $QtVersion + " (MinGW 64-bit) manually and set QT_DIR / MINGW_DIR.")
    }

    $root = Get-ToolchainInstallRoot
    Write-Host ""
    Write-Host "Qt toolchain not found - auto-installing Qt $QtVersion + MinGW + Ninja" -ForegroundColor Yellow
    Write-Host "Install location : $root" -ForegroundColor Yellow
    Write-Host "Download size    : about 1 GB, this may take several minutes ..." -ForegroundColor Yellow
    Write-Host ""

    & $python -m pip install --quiet --upgrade aqtinstall
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install aqtinstall. Check your network connection / pip."
    }

    & $python -m aqt install-qt windows desktop $QtVersion win64_mingw -O $root
    if ($LASTEXITCODE -ne 0) { throw "aqt install-qt failed (network problem?)." }

    & $python -m aqt install-qt windows desktop $QtVersion win64_mingw -m qtmultimedia -O $root
    & $python -m aqt install-tool windows desktop tools_mingw1310 -O $root
    & $python -m aqt install-tool windows desktop tools_ninja -O $root

    Write-Host "Toolchain installed." -ForegroundColor Green
}

function Resolve-Toolchain {
    param(
        [string]$QtVersion = "6.8.3",
        [switch]$AutoInstall
    )

    $qt = Find-Qt
    $mingw = Find-Mingw -QtDir $qt
    $ninja = Find-Ninja -QtDir $qt

    if ((-not $qt) -or (-not $mingw) -or (-not $ninja)) {
        if ($AutoInstall) {
            Install-Toolchain -QtVersion $QtVersion
            $qt = Find-Qt
            $mingw = Find-Mingw -QtDir $qt
            $ninja = Find-Ninja -QtDir $qt
        }
    }

    if (-not $qt) {
        throw ("Qt 6 not found. Options:" + [Environment]::NewLine +
               "  1) re-run without -NoAutoInstall to let this script download Qt" + [Environment]::NewLine +
               "  2) install Qt " + $QtVersion + " (MinGW 64-bit) and set QT_DIR, e.g." + [Environment]::NewLine +
               "     `$env:QT_DIR = 'C:\Qt\$QtVersion\mingw_64'")
    }
    if (-not $mingw) {
        throw ("MinGW-w64 (g++) not found. Install it under <QtRoot>\Tools\mingw*, or set MINGW_DIR.")
    }

    return [pscustomobject]@{
        QtDir     = $qt
        MingwDir  = $mingw
        NinjaDir  = $ninja
        QtVersion = $QtVersion
    }
}
