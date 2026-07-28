#Requires -Version 5.1
#Requires -RunAsAdministrator

<#
.SYNOPSIS
    TheSeed Auto-Build Script
    Erkennt automatisch das Laufwerk (C:, D:, etc.)

.DESCRIPTION
    Dieses Script:
    1. Erkennt automatisch den Projektordner
    2. Installiert vcpkg falls nicht vorhanden
    3. Installiert Dependencies
    4. Konfiguriert CMake mit vcpkg Toolchain
    5. Baut das Projekt
    6. Startet den Server
#>

param(
    [string]$ProjectPath = ""
)

$ErrorActionPreference = "Stop"

function Write-Info    { param([string]$msg) Write-Host "[INFO]    $msg" -ForegroundColor Cyan }
function Write-Success { param([string]$msg) Write-Host "[OK]      $msg" -ForegroundColor Green }
function Write-Warning { param([string]$msg) Write-Host "[WARN]    $msg" -ForegroundColor Yellow }
function Write-Error   { param([string]$msg) Write-Host "[ERROR]   $msg" -ForegroundColor Red }

Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║     TheSeed Auto-Build Script                                ║
║     Erkennt Laufwerk automatisch (C:, D:, etc.)              ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

# ============================================================
# 1. Projektverzeichnis finden
# ============================================================
if (-not $ProjectPath) {
    # Versuche automatisch zu finden
    $possiblePaths = @(
        "D:\dev\TheSeed",
        "C:\dev\TheSeed",
        "E:\dev\TheSeed",
        "$env:USERPROFILE\dev\TheSeed",
        "$env:USERPROFILE\Documents\TheSeed",
        "$env:USERPROFILE\Desktop\TheSeed"
    )

    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $ProjectPath = $path
            Write-Success "Projekt gefunden: $ProjectPath"
            break
        }
    }

    if (-not $ProjectPath) {
        Write-Error "Projekt nicht gefunden. Bitte gib -ProjectPath an."
        Write-Info "Beispiel: .\Auto-Build.ps1 -ProjectPath 'D:\dev\TheSeed'"
        exit 1
    }
} else {
    if (-not (Test-Path $ProjectPath)) {
        Write-Error "Projekt nicht gefunden: $ProjectPath"
        exit 1
    }
    Write-Success "Projekt: $ProjectPath"
}

Set-Location $ProjectPath
$drive = (Get-Item $ProjectPath).PSDrive.Name
Write-Info "Laufwerk: ${drive}:"

# ============================================================
# 2. Pruefe vcpkg
# ============================================================
Write-Info "Pruefe vcpkg..."

$vcpkgExe = Join-Path $ProjectPath "vcpkg\vcpkg.exe"
$vcpkgDir = Join-Path $ProjectPath "vcpkg"

if (-not (Test-Path $vcpkgExe)) {
    Write-Warning "vcpkg nicht gefunden. Installiere..."

    if (Test-Path $vcpkgDir) {
        Write-Info "Loese altes vcpkg..."
        Remove-Item -Recurse -Force $vcpkgDir
    }

    Write-Info "Klone vcpkg..."
    git clone https://github.com/Microsoft/vcpkg.git $vcpkgDir
    if ($LASTEXITCODE -ne 0) {
        Write-Error "git clone fehlgeschlagen. Pruefe Internetverbindung."
        exit 1
    }

    Write-Info "Bootstrap vcpkg..."
    $bootstrap = Join-Path $vcpkgDir "bootstrap-vcpkg.bat"
    & $bootstrap
    if ($LASTEXITCODE -ne 0) {
        Write-Error "vcpkg Bootstrap fehlgeschlagen."
        exit 1
    }

    Write-Success "vcpkg installiert"
} else {
    Write-Success "vcpkg gefunden"
}

# ============================================================
# 3. Umgebungsvariable
# ============================================================
Write-Info "Setze VCPKG_ROOT..."
$vcpkgRoot = $vcpkgDir
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
$env:VCPKG_ROOT = $vcpkgRoot
Write-Success "VCPKG_ROOT = $vcpkgRoot"

# ============================================================
# 4. vcpkg integrate
# ============================================================
Write-Info "vcpkg integrate install..."
& $vcpkgExe integrate install
Write-Success "Integriert"

# ============================================================
# 5. Dependencies installieren
# ============================================================
Write-Info "Installiere Dependencies (30-60 Min)..."
Write-Info "Druecke STRG+C zum Abbrechen, oder warte..."
Start-Sleep -Seconds 3

& $vcpkgExe install
if ($LASTEXITCODE -ne 0) {
    Write-Error "vcpkg install fehlgeschlagen."
    exit 1
}
Write-Success "Dependencies installiert"

# ============================================================
# 6. Alten Build loeschen
# ============================================================
$buildDir = Join-Path $ProjectPath "build"
if (Test-Path $buildDir) {
    Write-Info "Loese alten build..."
    Remove-Item -Recurse -Force $buildDir
    Write-Success "build geloescht"
}

# ============================================================
# 7. CMake konfigurieren
# ============================================================
Write-Info "CMake konfigurieren..."

$toolchain = Join-Path $vcpkgDir "scripts\buildsystems\vcpkg.cmake"
Write-Info "Toolchain: $toolchain"

cmake -G "Visual Studio 18 2026" -A x64 -B $buildDir -S $ProjectPath -DCMAKE_TOOLCHAIN_FILE="$toolchain"
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake fehlgeschlagen."
    Write-Info "Pruefe:"
    Write-Info "- CMake 4.2+ (fuer VS 18 2026)"
    Write-Info "- Visual Studio 2026 installiert"
    Write-Info "- C++ Workload in VS 2026"
    exit 1
}
Write-Success "CMake konfiguriert"

# ============================================================
# 8. Build
# ============================================================
Write-Info "Build..."
cmake --build $buildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build fehlgeschlagen."
    exit 1
}
Write-Success "Build erfolgreich"

# ============================================================
# 9. Tests
# ============================================================
Write-Info "Tests..."
ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Einige Tests fehlgeschlagen"
} else {
    Write-Success "Alle Tests bestanden"
}

# ============================================================
# 10. Server starten
# ============================================================
$serverExe = Join-Path $buildDir "Release\TheSeedServer.exe"

Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║                    BUILD ERFOLGREICH!                        ║
╚══════════════════════════════════════════════════════════════╝

Server: $serverExe

Starten mit ENTER, oder 'n' zum Beenden:
"@ -ForegroundColor Green

$start = Read-Host "Server starten?"
if ($start -ne "n" -and (Test-Path $serverExe)) {
    & $serverExe
} else {
    Write-Info "Server nicht gestartet."
    Write-Info "Manuell starten: $serverExe"
}
