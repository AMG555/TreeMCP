# TreeMCP One-Click Installer & Upgrade Script for Windows
$ErrorActionPreference = 'Stop'

# 1. Stop all previous/active TreeMCP processes before updating
Get-Process -Name "TreeMCP" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$installDir = "$env:LOCALAPPDATA\TreeMCP"
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
}

$exePath = Join-Path $installDir "TreeMCP.exe"
$tempNewExe = Join-Path $installDir "TreeMCP_new.exe"
$backupExe = Join-Path $installDir "TreeMCP.exe.bak"

# 2. Acquire the new binary cleanly (from local build or remote latest release)
if (Test-Path "$PSScriptRoot\TreeMCP.exe") {
    Write-Host "==> Installing latest local TreeMCP build..." -ForegroundColor Cyan
    Copy-Item -Path "$PSScriptRoot\TreeMCP.exe" -Destination $tempNewExe -Force
} else {
    Write-Host "==> Downloading latest TreeMCP release from GitHub..." -ForegroundColor Cyan
    $downloadUrl = "https://github.com/AMG555/TreeMCP/releases/latest/download/TreeMCP.exe"
    Invoke-WebRequest -Uri $downloadUrl -OutFile $tempNewExe -UseBasicParsing
}

# 3. Verify downloaded file integrity before replacing existing installation
if (-not (Test-Path $tempNewExe) -or (Get-Item $tempNewExe).Length -lt 50000) {
    throw "Downloaded binary is missing or corrupted. Aborting upgrade to protect existing installation."
}
Unblock-File -Path $tempNewExe -ErrorAction SilentlyContinue

# 4. Atomic file swap: preserve previous as .bak in case of unexpected lock
if (Test-Path $exePath) {
    try {
        Move-Item -Path $exePath -Destination $backupExe -Force
    } catch {
        # If locked by an elusive handle, retry after process purge
        Get-Process -Name "TreeMCP" -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Sleep -Milliseconds 300
        Move-Item -Path $exePath -Destination $backupExe -Force
    }
}

Move-Item -Path $tempNewExe -Destination $exePath -Force
Remove-Item -Path $backupExe -Force -ErrorAction SilentlyContinue

# 5. Clean up any stale or leftover binaries from old updates
Get-ChildItem -Path $installDir -Filter "TreeMCP*.exe" -File -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.FullName -ne $exePath) {
        Remove-Item -Path $_.FullName -Force -ErrorAction SilentlyContinue
    }
}
Get-ChildItem -Path $installDir -Filter "*.bak" -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# 6. Clean up any duplicate installations in legacy locations
$legacyPaths = @(
    "$env:ProgramFiles\TreeMCP",
    "${env:ProgramFiles(x86)}\TreeMCP",
    "$env:APPDATA\TreeMCP"
)
foreach ($lp in $legacyPaths) {
    if (Test-Path $lp) {
        Remove-Item -Path $lp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Ensure PATH includes install directory so `TreeMCP.exe` is callable from any shell
try {
    $userPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
    if ($userPath -notlike "*$installDir*") {
        [Environment]::SetEnvironmentVariable("Path", "$installDir;$userPath", [EnvironmentVariableTarget]::User)
        $env:Path = "$installDir;$env:Path"
        Write-Host "==> Added $installDir to User PATH" -ForegroundColor Green
    }
} catch {
    # Non-fatal
}

Write-Host "==> TreeMCP canonical binary verified at: $exePath" -ForegroundColor Green

# 7. Standard Windows Control Panel "Add/Remove Programs" Registration (merged version info)
try {
    $regPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\TreeMCP"
    if (-not (Test-Path $regPath)) {
        New-Item -Path $regPath -Force | Out-Null
    }
    Set-ItemProperty -Path $regPath -Name "DisplayName" -Value "TreeMCP - Disk Space Analyzer & MCP Server" -Force
    Set-ItemProperty -Path $regPath -Name "DisplayVersion" -Value "1.1.4" -Force
    Set-ItemProperty -Path $regPath -Name "Publisher" -Value "TreeMCP Project" -Force
    Set-ItemProperty -Path $regPath -Name "InstallLocation" -Value $installDir -Force
    Set-ItemProperty -Path $regPath -Name "DisplayIcon" -Value "$exePath,0" -Force
    Set-ItemProperty -Path $regPath -Name "UninstallString" -Value "powershell.exe -ExecutionPolicy Bypass -Command `"& '$installDir\uninstall.ps1'`"" -Force
    Set-ItemProperty -Path $regPath -Name "NoModify" -Value 1 -Type DWord -Force
    Set-ItemProperty -Path $regPath -Name "NoRepair" -Value 1 -Type DWord -Force

    # Copy self-contained uninstaller into installation directory
    if (Test-Path "$PSScriptRoot\uninstall.ps1") {
        Copy-Item -Path "$PSScriptRoot\uninstall.ps1" -Destination (Join-Path $installDir "uninstall.ps1") -Force
    }
    Write-Host "==> Registered single authoritative version in Windows Apps & Features" -ForegroundColor Green
} catch {
    # Non-fatal if registry fails
}

# 5. Authoritative Start Menu Application Shortcut
try {
    $shortcutPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\TreeMCP.lnk"
    $wsh = New-Object -ComObject WScript.Shell
    $shortcut = $wsh.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $exePath
    $shortcut.WorkingDirectory = $installDir
    $shortcut.Description = "TreeMCP - High Speed Disk Space Analyzer & MCP Server"
    $shortcut.Save()
    Write-Host "==> Start Menu shortcut created: $shortcutPath" -ForegroundColor Green
} catch {
    # Non-fatal if shortcut creation fails
}

# Register in MCP config files if present
$mcpConfigFiles = @(
    "$env:USERPROFILE\.gemini\config\mcp_config.json",
    "$env:APPDATA\Claude\claude_desktop_config.json",
    "$env:USERPROFILE\.cursor\mcp.json"
)

foreach ($cfgPath in $mcpConfigFiles) {
    if (Test-Path $cfgPath) {
        try {
            $raw = Get-Content $cfgPath -Raw -ErrorAction SilentlyContinue
            $cfg = if ($raw) { $raw | ConvertFrom-Json } else { [PSCustomObject]@{} }

            if (-not $cfg.mcpServers) {
                $cfg | Add-Member -MemberType NoteProperty -Name "mcpServers" -Value ([PSCustomObject]@{}) -Force
            }

            $serverObj = [PSCustomObject]@{
                command = $exePath
                args = @("--mcp")
            }

            $cfg.mcpServers | Add-Member -MemberType NoteProperty -Name "treemcp" -Value $serverObj -Force
            $cfg | ConvertTo-Json -Depth 10 | Set-Content $cfgPath -Encoding UTF8
            Write-Host "==> Registered treemcp in: $cfgPath" -ForegroundColor Green
        } catch {
            Write-Host "==> Skipped config $cfgPath (manual setup needed)" -ForegroundColor Yellow
        }
    }
}

Write-Host "`n[SUCCESS] TreeMCP installed successfully!" -ForegroundColor Green
Write-Host "Run GUI: & '$exePath'"
Write-Host "Run MCP: & '$exePath' --mcp`n"
