# ---------------------------------------------------------------------------
# make_source_zip.ps1 - Bundle the complete project sources into one archive.
#
#   powershell -ExecutionPolicy Bypass -File .\tools\make_source_zip.ps1
#   -> dist\ShanCi-source.zip
#
# Includes everything needed to build on another computer:
#   src/ tests/ tools/ scripts/ docs/ third_party/ data/dict/ *.ps1 *.py *.md
# Excludes build outputs and local state: build/ dist/ .git/ .buildroot
#
# This file must stay ASCII-only (Windows PowerShell 5.1 reads .ps1 as ANSI).
# ---------------------------------------------------------------------------
param(
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $root "dist\ShanCi-source.zip"
}

Write-Host "Project root : $root"
Write-Host "Output       : $Output"

$staging = Join-Path $env:TEMP ("shanci-src-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $staging | Out-Null

try {
    $xd = @(
        (Join-Path $root "build"),
        (Join-Path $root "dist"),
        (Join-Path $root ".git"),
        (Join-Path $root ".vs"),
        (Join-Path $root ".idea"),
        (Join-Path $root ".vscode")
    )
    $args = @($root, $staging, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP", "/XF", ".buildroot")
    foreach ($d in $xd) { $args += "/XD"; $args += $d }

    & robocopy @args | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

    $null = New-Item -ItemType Directory -Force -Path (Split-Path $Output) 
    if (Test-Path -LiteralPath $Output) { Remove-Item -LiteralPath $Output -Force }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $staging, $Output, [System.IO.Compression.CompressionLevel]::Optimal, $false)

    $files = (Get-ChildItem -Path $staging -Recurse -File | Measure-Object).Count
    $sizeMb = [math]::Round((Get-Item $Output).Length / 1MB, 2)
    Write-Host ""
    Write-Host ("Done: " + $files + " files -> " + $Output + "  (" + $sizeMb + " MB)") -ForegroundColor Green
} finally {
    if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
}
