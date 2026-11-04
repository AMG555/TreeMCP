# TreeMCP One-Click Uninstaller for Windows
$ErrorActionPreference = 'SilentlyContinue'

Write-Host "==> Stopping any active TreeMCP processes..." -ForegroundColor Cyan
Get-Process -Name "TreeMCP" -ErrorAction SilentlyContinue | Stop-Process -Force

# 1. Unregister from MCP configuration files
$mcpConfigFiles = @(
    "$env:USERPROFILE\.gemini\config\mcp_config.json",
    "$env:APPDATA\Claude\claude_desktop_config.json",
    "$env:USERPROFILE\.cursor\mcp.json"
)

foreach ($cfgPath in $mcpConfigFiles) {
    if (Test-Path $cfgPath) {
        try {
            $raw = Get-Content $cfgPath -Raw -ErrorAction SilentlyContinue
            if ($raw) {
                $cfg = $raw | ConvertFrom-Json
                if ($cfg.mcpServers -and $cfg.mcpServers.treemcp) {
                    $cfg.mcpServers.PSObject.Properties.Remove('treemcp')
                    $cfg | ConvertTo-Json -Depth 10 | Set-Content $cfgPath -Encoding UTF8
                    Write-Host "==> Removed treemcp registration from: $cfgPath" -ForegroundColor Green
                }
            }
        } catch {
            Write-Host "==> Could not modify $cfgPath" -ForegroundColor Yellow
        }
    }
}

# 2. Remove binary and installation folder
$installDir = "$env:LOCALAPPDATA\TreeMCP"
if (Test-Path $installDir) {
    Remove-Item -Recurse -Force $installDir -ErrorAction SilentlyContinue
    Write-Host "==> Deleted installation directory: $installDir" -ForegroundColor Green
}

Write-Host "`n[SUCCESS] TreeMCP completely uninstalled!" -ForegroundColor Green
