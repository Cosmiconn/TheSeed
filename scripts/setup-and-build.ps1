# TheSeed - Windows Setup & Build Script v3.0
# Prueft alle Abhaengigkeiten, installiert bei Bedarf, baut und testet.
# Ausfuehren als: powershell -ExecutionPolicy Bypass -File scripts\setup-and-build.ps1
# oder direkt in PowerShell 7+: .\scripts\setup-and-build.ps1

param(
    [string]$Preset = "windows-release",
    [switch]$SkipSetup,
    [switch]$SkipBuild,
    [switch]$SkipTest,
    [switch]$Sanitizers,
    [switch]$InstallDeps,
    [switch]$Help
)

if ($Help) {
    Write-Host @"
TheSeed Windows Setup & Build Script

Nutzung:
  .\scripts\setup-and-build.ps1 [Optionen]

Optionen:
  -Preset <name>     Build-Preset (windows-release, windows-debug) [Standard: windows-release]
  -SkipSetup         Setup-Phase ueberspringen
  -SkipBuild         Build-Phase ueberspringen
  -SkipTest          Test-Phase ueberspringen
  -Sanitizers        Sanitizer im Build aktivieren
  -InstallDeps       vcpkg und Dependencies automatisch installieren
  -Help              Diese Hilfe anzeigen

Beispiele:
  .\scripts\setup-and-build.ps1 -InstallDeps
  .\scripts\setup-and-build.ps1 -Preset windows-debug -Sanitizers
  .\scripts\setup-and-build.ps1 -SkipSetup -SkipTest
"@
    exit 0
}

$ErrorActionPreference = "Stop"
$script:MissingCritical = $false

# ============================================================================
# Farbige Ausgabe
# ============================================================================
function Write-Header($text) {
    Write-Host "`n=== $text ===" -ForegroundColor Cyan
}
function Write-Ok($text) { Write-Host "  [OK] $text" -ForegroundColor Green }
function Write-Warn($text) { Write-Host "  [WARN] $text" -ForegroundColor Yellow }
function Write-Fail($text) { Write-Host "  [FAIL] $text" -ForegroundColor Red }
function Write-Info($text) { Write-Host "  [INFO] $text" -ForegroundColor White }
function Write-Step($text) { Write-Host "`n>> $text" -ForegroundColor Magenta }

# ============================================================================
# Hilfsfunktion: Umgebung aus Batch-File laden (ROBUST)
# ============================================================================
function Import-VSEnvironment {
    param([string]$BatchFile)
    Write-Info "Lade Umgebung aus: $BatchFile"

    if (-not (Test-Path $BatchFile)) {
        Write-Warn "Batch-File nicht gefunden: $BatchFile"
        return $false
    }

    $tempBat = [System.IO.Path]::GetTempFileName() + ".bat"
    $tempOut = [System.IO.Path]::GetTempFileName()
    $batchName = [System.IO.Path]::GetFileName($BatchFile)

    # Parameter je nach Batch-Typ
    if ($batchName -eq "VsDevCmd.bat") {
        $params = "-arch=x64 -host_arch=x64"
    } elseif ($batchName -eq "vcvarsall.bat") {
        $params = "x64"
    } else {
        $params = ""
    }

    # Erstelle temporäres Batch-File das das VS-Batch sourct und set ausfuehrt
    $batContent = "@echo off`r`ncall `"$BatchFile`" $params >nul 2>&1`r`nset > `"$tempOut`""
    [System.IO.File]::WriteAllText($tempBat, $batContent, [System.Text.Encoding]::ASCII)

    # Fuehre das temporäre Batch-File aus
    & cmd /c "`"$tempBat`""
    $exitCode = $LASTEXITCODE

    Remove-Item $tempBat -ErrorAction SilentlyContinue

    if ($exitCode -ne 0) {
        Write-Warn "Batch-File exit code: $exitCode"
    }

    if (-not (Test-Path $tempOut)) {
        Write-Warn "Temp-Ausgabedatei nicht erstellt"
        return $false
    }

    $lines = Get-Content $tempOut -ErrorAction SilentlyContinue
    Remove-Item $tempOut -ErrorAction SilentlyContinue

    if (-not $lines) {
        Write-Warn "Keine Umgebungsvariablen gelesen"
        return $false
    }

    $count = 0
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') {
            $name = $Matches[1].Trim()
            $value = $Matches[2].Trim()
            if ($name -and $name -notin @("PROMPT","PSModulePath","TMP","TEMP")) {
                [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
                $count++
            }
        }
    }

    Write-Info "Uebernommene Variablen: $count"

    # Debug-Ausgabe
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    $rc = Get-Command rc -ErrorAction SilentlyContinue
    $lib = $env:LIB
    Write-Info "Nach Laden: cl.exe=$($cl -ne $null), rc.exe=$($rc -ne $null), LIB=$($lib -ne $null)"

    return ($cl -ne $null)
}

# ============================================================================
# Hilfsfunktion: Windows SDK finden (rc.exe, mt.exe, lib)
# ============================================================================
function Find-WindowsSdkTools {
    Write-Info "Suche Windows SDK (rc.exe, lib)..."

    $sdkRoot = $null
    $sdkVersion = $null

    # Suche 1: rc.exe in bekannten Pfaden
    $searchPaths = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin",
        "C:\Program Files (x86)\Windows Kits\10\bin",
        "C:\Program Files\Windows Kits\10\bin",
        "${env:ProgramFiles(x86)}\Windows Kits\11\bin",
        "${env:ProgramFiles}\Windows Kits\11\bin"
    )

    foreach ($base in $searchPaths) {
        if (-not (Test-Path $base)) { continue }

        $versions = Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^10\.|^11\.' } |
            Sort-Object Name -Descending

        foreach ($ver in $versions) {
            $rc64 = Join-Path $ver.FullName "x64\rc.exe"
            $rc32 = Join-Path $ver.FullName "x86\rc.exe"

            if (Test-Path $rc64) {
                $sdkBin = Join-Path $ver.FullName "x64"
                $env:PATH = "$sdkBin;$env:PATH"
                $sdkRoot = Split-Path (Split-Path $ver.FullName) -Parent
                $sdkVersion = $ver.Name
                Write-Ok "Windows SDK x64 gefunden: $sdkBin"
                break
            }
            if (Test-Path $rc32) {
                $sdkBin = Join-Path $ver.FullName "x86"
                $env:PATH = "$sdkBin;$env:PATH"
                $sdkRoot = Split-Path (Split-Path $ver.FullName) -Parent
                $sdkVersion = $ver.Name
                Write-Ok "Windows SDK x86 gefunden: $sdkBin"
                break
            }
        }
        if ($sdkRoot) { break }
    }

    # Suche 2: Windows SDK Lib-Pfade (kernel32.lib etc.)
    if ($sdkRoot -and $sdkVersion) {
        $libPaths = @(
            "$sdkRoot\Lib\$sdkVersion\um\x64",
            "$sdkRoot\Lib\$sdkVersion\ucrt\x64"
        )
        foreach ($lp in $libPaths) {
            if (Test-Path $lp) {
                if ($env:LIB) {
                    $env:LIB = "$lp;$env:LIB"
                } else {
                    $env:LIB = $lp
                }
                Write-Info "SDK Lib-Pfad hinzugefuegt: $lp"
            }
        }

        $includePaths = @(
            "$sdkRoot\Include\$sdkVersion\um",
            "$sdkRoot\Include\$sdkVersion\ucrt",
            "$sdkRoot\Include\$sdkVersion\shared"
        )
        foreach ($ip in $includePaths) {
            if (Test-Path $ip) {
                if ($env:INCLUDE) {
                    $env:INCLUDE = "$ip;$env:INCLUDE"
                } else {
                    $env:INCLUDE = $ip
                }
            }
        }
    }

    # Suche 3: Fallback - direkte Suche nach kernel32.lib
    if (-not $env:LIB -or -not (Test-Path (Join-Path ($env:LIB -split ';' | Select-Object -First 1) "kernel32.lib"))) {
        Write-Info "Suche kernel32.lib direkt..."
        $libSearchPaths = @(
            "${env:ProgramFiles(x86)}\Windows Kits\10\Lib",
            "${env:ProgramFiles}\Windows Kits\10\Lib",
            "C:\Program Files (x86)\Windows Kits\10\Lib",
            "C:\Program Files\Windows Kits\10\Lib"
        )
        foreach ($libBase in $libSearchPaths) {
            if (-not (Test-Path $libBase)) { continue }
            $versions = Get-ChildItem $libBase -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
            foreach ($ver in $versions) {
                $umLib = Join-Path $ver.FullName "um\x64"
                $ucrtLib = Join-Path $ver.FullName "ucrt\x64"
                if ((Test-Path "$umLib\kernel32.lib") -and (Test-Path "$ucrtLib\ucrt.lib")) {
                    $libPaths = @($umLib, $ucrtLib)
                    if ($env:LIB) {
                        $env:LIB = ($libPaths -join ";") + ";$env:LIB"
                    } else {
                        $env:LIB = $libPaths -join ";"
                    }
                    Write-Ok "kernel32.lib gefunden in: $umLib"
                    break
                }
            }
        }
    }

    # Suche 4: MSVC Runtime Libs (MSVCRTD.lib / MSVCRT.lib)
    $msvcLibFound = $false
    if ($env:LIB) {
        $msvcLibFound = ($env:LIB -split ';' | Where-Object { 
            (Test-Path (Join-Path $_ "MSVCRTD.lib")) -or (Test-Path (Join-Path $_ "MSVCRT.lib"))
        }) -ne $null
    }
    if (-not $msvcLibFound) {
        Write-Info "Suche MSVC Runtime Libs (MSVCRTD.lib)..."
        $msvcLibPatterns = @(
            "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\*\lib\x64",
            "${env:ProgramFiles}\Microsoft Visual Studio\2026\*\VC\Tools\MSVC\*\lib\x64",
            "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\lib\x64",
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\lib\x64"
        )
        foreach ($pattern in $msvcLibPatterns) {
            $matches = Get-Item $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
            if ($matches -and (Test-Path "$($matches.FullName)\MSVCRTD.lib")) {
                $msvcLibDir = $matches.FullName
                if ($env:LIB) {
                    $env:LIB = "$msvcLibDir;$env:LIB"
                } else {
                    $env:LIB = $msvcLibDir
                }
                Write-Ok "MSVC Runtime Libs gefunden: $msvcLibDir"
                $msvcLibFound = $true
                break
            }
        }
    }

    $success = ($sdkRoot -ne $null) -or $msvcLibFound
    if (-not $success) {
        Write-Warn "Weder Windows SDK noch MSVC Runtime Libs gefunden"
    }
    return $success
}

# ============================================================================
# Pruefung: PowerShell Version
# ============================================================================
function Test-PowerShellVersion {
    Write-Header "PowerShell Version"
    $ver = $PSVersionTable.PSVersion
    Write-Info "PowerShell $($ver.Major).$($ver.Minor).$($ver.Patch)"
    if ($ver.Major -lt 5) {
        Write-Fail "PowerShell 5+ erforderlich"
        $script:MissingCritical = $true
        return $false
    }
    if ($ver.Major -lt 7) {
        Write-Warn "PowerShell 7+ empfohlen"
    } else {
        Write-Ok "PowerShell 7+"
    }
    return $true
}

# ============================================================================
# Pruefung: Git
# ============================================================================
function Test-Git {
    Write-Header "Git"
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $ver = & git --version 2>$null
        Write-Ok "$ver gefunden"
        return $true
    }
    Write-Fail "Git nicht gefunden"
    Write-Info "Installieren: winget install Git.Git"
    $script:MissingCritical = $true
    return $false
}

# ============================================================================
# Pruefung: CMake (>= 3.25)
# ============================================================================
function Test-CMake {
    Write-Header "CMake"
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        $verStr = & cmake --version 2>$null | Select-Object -First 1
        if ($verStr -match '(\d+)\.(\d+)\.(\d+)') {
            $major = [int]$Matches[1]
            $minor = [int]$Matches[2]
            if ($major -gt 3 -or ($major -eq 3 -and $minor -ge 25)) {
                Write-Ok "$verStr (>= 3.25)"
                return $true
            }
            Write-Fail "$verStr gefunden, aber >= 3.25 benoetigt"
        }
    } else {
        Write-Fail "CMake nicht gefunden"
    }
    Write-Info "Installieren: winget install Kitware.CMake"
    $script:MissingCritical = $true
    return $false
}

# ============================================================================
# Pruefung: Ninja
# ============================================================================
function Test-Ninja {
    Write-Header "Ninja"
    $ninja = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninja) {
        $ver = & ninja --version 2>$null
        Write-Ok "Ninja $ver gefunden"
        return $true
    }

    # Suche in VS-Installationen
    $vsNinjaPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    )

    foreach ($path in $vsNinjaPaths) {
        if (Test-Path $path) {
            $dir = [System.IO.Path]::GetDirectoryName($path)
            $env:PATH = "$dir;$env:PATH"
            $ver = & $path --version 2>$null
            Write-Ok "Ninja $ver (aus VS) gefunden"
            return $true
        }
    }

    Write-Fail "Ninja nicht gefunden"
    Write-Info "Installieren: winget install Ninja-build.Ninja"
    $script:MissingCritical = $true
    return $false
}

# ============================================================================
# Pruefung: MSVC / Visual Studio Build Tools
# ============================================================================
function Test-MSVC {
    Write-Header "MSVC / Visual Studio Build Tools"

    # Bereits verfuegbar?
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    if ($cl) {
        Write-Ok "cl.exe bereits verfuegbar"
        $rc = Get-Command rc -ErrorAction SilentlyContinue
        $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { 
            (Test-Path (Join-Path $_ "kernel32.lib")) -and 
            ((Test-Path (Join-Path $_ "MSVCRTD.lib")) -or (Test-Path (Join-Path $_ "MSVCRT.lib")))
        }))
        if (-not $rc -or -not $libOk) {
            Find-WindowsSdkTools | Out-Null
        }
        $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { 
            (Test-Path (Join-Path $_ "kernel32.lib")) -and 
            ((Test-Path (Join-Path $_ "MSVCRTD.lib")) -or (Test-Path (Join-Path $_ "MSVCRT.lib")))
        }))
        if ($libOk -and $rc) {
            return $true
        }
        # Direkte Suche nach MSVC-Libs als letzter Versuch
        if (-not $libOk) {
            Write-Warn "cl.exe OK, aber MSVC-LIBs fehlen - suche direkt"
            $msvcLibPatterns = @(
                "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\*\lib\x64",
                "${env:ProgramFiles}\Microsoft Visual Studio\2026\*\VC\Tools\MSVC\*\lib\x64",
                "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\lib\x64",
                "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\lib\x64"
            )
            foreach ($pattern in $msvcLibPatterns) {
                $matches = Get-Item $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
                if ($matches -and (Test-Path "$($matches.FullName)\MSVCRTD.lib")) {
                    $msvcLibDir = $matches.FullName
                    if ($env:LIB) {
                        $env:LIB = "$msvcLibDir;$env:LIB"
                    } else {
                        $env:LIB = $msvcLibDir
                    }
                    Write-Ok "MSVC Runtime Libs direkt gefunden: $msvcLibDir"
                    $libOk = $true
                    break
                }
            }
        }
        $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { 
            (Test-Path (Join-Path $_ "kernel32.lib")) -and 
            ((Test-Path (Join-Path $_ "MSVCRTD.lib")) -or (Test-Path (Join-Path $_ "MSVCRT.lib")))
        }))
        if ($libOk -and $rc) {
            return $true
        }
        Write-Warn "cl.exe OK, aber MSVC-LIBs oder rc.exe fehlen - lade VS-Umgebung"
    }

    # Sammle alle VS-Installationen
    $vsInstalls = @()

    # 1. vswhere
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $found = & $vsWhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($found) { $vsInstalls += $found }
    }

    # 2. Bekannte Pfade
    $knownPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Community",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Professional",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Enterprise",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\BuildTools",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
    )
    foreach ($kp in $knownPaths) {
        if (Test-Path $kp) { $vsInstalls += $kp }
    }

    $vsInstalls = $vsInstalls | Select-Object -Unique
    Write-Info "Gefundene VS-Installationen: $($vsInstalls.Count)"

    foreach ($install in $vsInstalls) {
        Write-Info "Versuche: $install"

        # A) VsDevCmd.bat
        $devCmd = Join-Path $install "Common7\Tools\VsDevCmd.bat"
        if (Test-Path $devCmd) {
            if (Import-VSEnvironment -BatchFile $devCmd) {
                Find-WindowsSdkTools | Out-Null
                $rc = Get-Command rc -ErrorAction SilentlyContinue
                $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { Test-Path (Join-Path $_ "kernel32.lib") }))
                if ($rc -and $libOk) {
                    Write-Ok "MSVC + SDK ueber VsDevCmd.bat geladen"
                    return $true
                }
                if (-not $rc) { Write-Warn "cl.exe OK, aber rc.exe fehlt nach VsDevCmd.bat" }
                if (-not $libOk) { Write-Warn "cl.exe OK, aber LIB fehlt nach VsDevCmd.bat" }
            }
        }

        # B) vcvars64.bat
        $vcvars64 = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars64) {
            if (Import-VSEnvironment -BatchFile $vcvars64) {
                Find-WindowsSdkTools | Out-Null
                $rc = Get-Command rc -ErrorAction SilentlyContinue
                $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { Test-Path (Join-Path $_ "kernel32.lib") }))
                if ($rc -and $libOk) {
                    Write-Ok "MSVC + SDK ueber vcvars64.bat geladen"
                    return $true
                }
                if (-not $rc) { Write-Warn "cl.exe OK, aber rc.exe fehlt nach vcvars64.bat" }
                if (-not $libOk) { Write-Warn "cl.exe OK, aber LIB fehlt nach vcvars64.bat" }
            }
        }

        # C) vcvarsall.bat
        $vcvarsall = Join-Path $install "VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path $vcvarsall) {
            if (Import-VSEnvironment -BatchFile $vcvarsall) {
                Find-WindowsSdkTools | Out-Null
                $rc = Get-Command rc -ErrorAction SilentlyContinue
                $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { Test-Path (Join-Path $_ "kernel32.lib") }))
                if ($rc -and $libOk) {
                    Write-Ok "MSVC + SDK ueber vcvarsall.bat geladen"
                    return $true
                }
                if (-not $rc) { Write-Warn "cl.exe OK, aber rc.exe fehlt nach vcvarsall.bat" }
                if (-not $libOk) { Write-Warn "cl.exe OK, aber LIB fehlt nach vcvarsall.bat" }
            }
        }
    }

    # 3. Direkte Suche nach cl.exe (letzter Versuch)
    Write-Info "Suche direkt nach cl.exe..."
    $clPatterns = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
    )
    $foundCl = $null
    foreach ($pat in $clPatterns) {
        $items = Get-Item $pat -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if ($items) { $foundCl = $items.FullName; break }
    }

    if ($foundCl) {
        Write-Ok "cl.exe gefunden: $foundCl"
        $clDir = [System.IO.Path]::GetDirectoryName($foundCl)
        $env:PATH = "$clDir;$env:PATH"
        Find-WindowsSdkTools | Out-Null
        $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { Test-Path (Join-Path $_ "kernel32.lib") }))
        if ($libOk) {
            Write-Ok "MSVC manuell konfiguriert"
            return $true
        }
        Write-Warn "cl.exe OK, aber LIB fehlt nach manueller Konfiguration"
    }

    Write-Fail "MSVC nicht gefunden"
    Write-Info "Installieren: winget install Microsoft.VisualStudio.2026.BuildTools"
    Write-Info "WICHTIG: Auch 'Windows 11 SDK' Workload auswaehlen!"
    $script:MissingCritical = $true
    return $false
}

# ============================================================================
# Pruefung: vcpkg
# ============================================================================
function Test-Vcpkg {
    Write-Header "vcpkg"

    if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
        $ver = & "$env:VCPKG_ROOT\vcpkg.exe" --version 2>$null | Select-Object -First 1
        Write-Ok "vcpkg gefunden: $env:VCPKG_ROOT ($ver)"
        return $true
    }

    $candidates = @(
        "C:\vcpkg",
        "$env:USERPROFILE\vcpkg",
        "$env:USERPROFILE\source\vcpkg",
        "$env:LOCALAPPDATA\vcpkg"
    )
    foreach ($c in $candidates) {
        if (Test-Path "$c\vcpkg.exe") {
            $env:VCPKG_ROOT = $c
            $ver = & "$c\vcpkg.exe" --version 2>$null | Select-Object -First 1
            Write-Ok "vcpkg gefunden: $c ($ver)"
            return $true
        }
    }

    if ($InstallDeps) {
        Write-Info "Klone vcpkg nach C:\vcpkg..."
        if (-not (Test-Path "C:\vcpkg")) {
            & git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg 2>$null
        }
        & C:\vcpkg\bootstrap-vcpkg.bat 2>$null
        $env:VCPKG_ROOT = "C:\vcpkg"
        Write-Ok "vcpkg installiert"
        return $true
    }

    Write-Fail "vcpkg nicht gefunden"
    Write-Info "Automatisch installieren: .\scripts\setup-and-build.ps1 -InstallDeps"
    Write-Info "Oder manuell:"
    Write-Info "  git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg"
    Write-Info "  C:\vcpkg\bootstrap-vcpkg.bat"
    $script:MissingCritical = $true
    return $false
}

# ============================================================================
# Optionale Tools
# ============================================================================
function Test-OptionalTools {
    Write-Header "Optionale Tools"
    $tools = @("clang-tidy", "cppcheck", "clang-format")
    foreach ($t in $tools) {
        $cmd = Get-Command $t -ErrorAction SilentlyContinue
        if ($cmd) { Write-Ok "$t gefunden" } else { Write-Warn "$t nicht gefunden (optional)" }
    }
}

# ============================================================================
# vcpkg Dependencies installieren
# ============================================================================
function Install-VcpkgDeps {
    Write-Step "Installiere vcpkg Dependencies..."
    $vcpkg = "$env:VCPKG_ROOT\vcpkg.exe"
    $triplet = "x64-windows"

    if (Test-Path "$PSScriptRoot\..\vcpkg.json") {
        Write-Info "Root-Dependencies..."
        & $vcpkg install --triplet=$triplet
        if ($LASTEXITCODE -ne 0) { throw "vcpkg install (root) fehlgeschlagen" }
    }

    $submodules = @("seed-core")
    foreach ($sm in $submodules) {
        $smVcpkg = "$PSScriptRoot\..\submodules\$sm\vcpkg.json"
        if (Test-Path $smVcpkg) {
            Write-Info "Dependencies fuer $sm..."
            Push-Location "$PSScriptRoot\..\submodules\$sm"
            & $vcpkg install --triplet=$triplet
            Pop-Location
            if ($LASTEXITCODE -ne 0) { throw "vcpkg install ($sm) fehlgeschlagen" }
        }
    }
    Write-Ok "Alle Dependencies installiert"
}

# ============================================================================
# Git Submodules
# ============================================================================
function Update-Submodules {
    Write-Step "Initialisiere Git Submodules..."
    Push-Location "$PSScriptRoot\.."
    if (Test-Path ".gitmodules") {
        & git submodule update --init --recursive
        if ($LASTEXITCODE -eq 0) {
            Write-Ok "Submodules initialisiert"
        } else {
            Write-Warn "Submodule-Update fehlgeschlagen"
        }
    } else {
        Write-Info "Keine .gitmodules gefunden - ueberspringe"
    }
    Pop-Location
}

# ============================================================================
# Build
# ============================================================================
function Invoke-Build {
    param([string]$Preset)
    Write-Step "Build mit Preset: $Preset"

    $buildDir = "build\$Preset"

    # Stelle sicher, dass MSVC verfuegbar ist
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    $libOk = ($env:LIB -and ($env:LIB -split ';' | Where-Object { Test-Path (Join-Path $_ "kernel32.lib") }))
    if (-not $cl -or -not $libOk) {
        Write-Warn "MSVC Umgebung unvollstaendig - lade nach..."
        Test-MSVC | Out-Null
    }

    $env:CMAKE_TOOLCHAIN_FILE = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

    # Pruefe ob rc.exe fehlt -> CMake Workaround
    $rc = Get-Command rc -ErrorAction SilentlyContinue
    $extraArgs = @()
    if (-not $rc) {
        Write-Warn "rc.exe fehlt - verwende CMake Workaround"
        $extraArgs += "-DCMAKE_RC_COMPILER=echo"
    }

    Write-Info "Configure..."
    if ($extraArgs.Count -gt 0) {
        cmake --preset $Preset @extraArgs
    } else {
        cmake --preset $Preset
    }
    if ($LASTEXITCODE -ne 0) { throw "CMake configure fehlgeschlagen" }

    Write-Info "Build..."
    cmake --build $buildDir --parallel
    if ($LASTEXITCODE -ne 0) { throw "Build fehlgeschlagen" }

    Write-Ok "Build erfolgreich: $buildDir"
}

# ============================================================================
# Test
# ============================================================================
function Invoke-Test {
    param([string]$Preset)
    Write-Step "Tests mit Preset: $Preset"

    $buildDir = "build\$Preset"

    Write-Info "Liste alle Tests..."
    ctest --test-dir $buildDir -N

    Write-Info "Fuehre Tests aus..."
    if ($Preset -like "*debug*" -or $Sanitizers) {
        $env:ASAN_OPTIONS = "detect_leaks=1:abort_on_error=1:print_stats=1"
        $env:UBSAN_OPTIONS = "print_stacktrace=1:halt_on_error=1"
    }
    ctest --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests fehlgeschlagen" }

    Write-Ok "Alle Tests bestanden"
}

# ============================================================================
# Stress Tests
# ============================================================================
function Invoke-StressTests {
    param([string]$Preset)
    Write-Step "Stress Tests..."
    $buildDir = "build\$Preset"

    $stressTests = @("Integration_MultiThreadStress", "Integration_100kEntities_Stress")
    foreach ($test in $stressTests) {
        Write-Info "Stress Test: $test (10x)..."
        for ($i = 1; $i -le 10; $i++) {
            Write-Host "  Run $i/10..." -NoNewline
            $result = ctest --test-dir $buildDir -R $test --output-on-failure 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host " FAIL" -ForegroundColor Red
                throw "Stress Test $test fehlgeschlagen bei Run $i"
            }
            Write-Host " OK" -ForegroundColor Green
        }
    }
    Write-Ok "Alle Stress Tests bestanden"
}

# ============================================================================
# Static Analysis
# ============================================================================
function Invoke-StaticAnalysis {
    Write-Step "Static Analysis..."
    $buildDir = "build\windows-debug"

    $clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($clangTidy -and (Test-Path "$buildDir\compile_commands.json")) {
        Write-Info "Running clang-tidy..."
        & clang-tidy -p $buildDir (Get-ChildItem "$PSScriptRoot\..\src" -Recurse -Filter "*.cpp" | Select-Object -ExpandProperty FullName)
        Write-Ok "clang-tidy abgeschlossen"
    } else {
        Write-Warn "clang-tidy uebersprungen"
    }

    $cppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
    if ($cppcheck) {
        Write-Info "Running cppcheck..."
        & cppcheck --enable=all `
            -I "$PSScriptRoot\..\src" `
            --suppress=missingIncludeSystem `
            --suppress=missingInclude `
            --suppress=unusedFunction `
            --error-exitcode=0 `
            "$PSScriptRoot\..\src" "$PSScriptRoot\..\tests"
        Write-Ok "cppcheck abgeschlossen"
    } else {
        Write-Warn "cppcheck uebersprungen"
    }
}

# ============================================================================
# MAIN
# ============================================================================
Write-Host @"
  _____ _           _     _____         _
 |_   _| |__   ___ | |_  | ____|_  ____| | ___
   | | | '_ \ / _ \| __| |  _| \ \/ / _` |/ _ \
   | | | | | | (_) | |_  | |___ >  < (_| |  __/
   |_| |_| |_|\___/ \__| |_____/_/\_\__,_|\___|

   Windows Setup & Build Script v3.0
"@ -ForegroundColor Cyan

Push-Location "$PSScriptRoot\.."
$startTime = Get-Date

try {
    # --- Setup Phase ---
    if (-not $SkipSetup) {
        Write-Step "SETUP PHASE"
        Test-PowerShellVersion | Out-Null
        Test-Git | Out-Null
        Test-CMake | Out-Null
        Test-Ninja | Out-Null
        Test-MSVC | Out-Null
        Test-Vcpkg | Out-Null
        Test-OptionalTools

        if ($script:MissingCritical) {
            Write-Host "`n" -NoNewline
            Write-Fail "KRITISCHE ABHAENGIGKEITEN FEHLEN!"
            Write-Info "Schnell-Install mit winget (als Admin):"
            Write-Info "  winget install Microsoft.PowerShell Git.Git Kitware.CMake Ninja-build.Ninja Microsoft.VisualStudio.2026.BuildTools"
            Write-Info "  winget install Microsoft.WindowsSDK"
            exit 1
        }

        if ($InstallDeps) {
            Install-VcpkgDeps
        }
        Update-Submodules
        Write-Ok "Setup Phase abgeschlossen"
    }

    # --- Build Phase ---
    if (-not $SkipBuild) {
        Write-Step "BUILD PHASE"
        Invoke-Build -Preset $Preset
    }

    # --- Test Phase ---
    if (-not $SkipTest) {
        Write-Step "TEST PHASE"
        Invoke-Test -Preset $Preset
        Invoke-StressTests -Preset $Preset
    }

    # --- Static Analysis ---
    if ($Preset -like "*debug*" -and -not $SkipTest) {
        Invoke-StaticAnalysis
    }

    $elapsed = (Get-Date) - $startTime
    Write-Host "`n" -NoNewline
    Write-Ok "ALLES ERFOLGREICH! Dauer: $($elapsed.ToString('mm\:ss'))"
    Write-Info "Binary: build\$Preset\seed_smoke.exe"

} catch {
    Write-Host "`n" -NoNewline
    Write-Fail "FEHLER: $_"
    Write-Info "Stacktrace: $($_.ScriptStackTrace)"
    exit 1
} finally {
    Pop-Location
}
