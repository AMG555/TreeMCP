# TreeMCP One-Click Installer for Windows
$ErrorActionPreference = 'Stop'

# Stop any running TreeMCP process before updating
Get-Process -Name "TreeMCP" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 200

$installDir = "$env:LOCALAPPDATA\TreeMCP"
if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
}

$exePath = Join-Path $installDir "TreeMCP.exe"

if (Test-Path "$PSScriptRoot\TreeMCP.exe") {
    Write-Host "==> Installing latest local TreeMCP build..." -ForegroundColor Cyan
    Copy-Item -Path "$PSScriptRoot\TreeMCP.exe" -Destination $exePath -Force
} else {
    Write-Host "==> Downloading TreeMCP standalone binary..." -ForegroundColor Cyan
    $downloadUrl = "https://github.com/AMG555/TreeMCP/releases/download/v1.1.0/TreeMCP.exe"
    Invoke-WebRequest -Uri $downloadUrl -OutFile $exePath -UseBasicParsing
}
Unblock-File -Path $exePath -ErrorAction SilentlyContinue

Write-Host "==> TreeMCP binary installed at: $exePath" -ForegroundColor Green

# Create Start Menu Application Shortcut
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
