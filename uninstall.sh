#!/usr/bin/env bash
# TreeMCP One-Click Uninstaller for Linux
set -euo pipefail

echo "==> Stopping any running treemcp processes..."
pkill -f treemcp || true

# 1. Unregister from Claude Desktop / MCP client config
CLAUDE_CONFIG="$HOME/.config/Claude/claude_desktop_config.json"
if [ -f "$CLAUDE_CONFIG" ]; then
    if command -v jq >/dev/null 2>&1; then
        TMP_JSON=$(mktemp)
        jq 'del(.mcpServers.treemcp)' "$CLAUDE_CONFIG" > "$TMP_JSON" && mv "$TMP_JSON" "$CLAUDE_CONFIG"
        echo "==> Removed treemcp from $CLAUDE_CONFIG"
    fi
fi

# 2. Remove binary
INSTALL_DIR="$HOME/.local/bin"
if [ -f "$INSTALL_DIR/treemcp" ]; then
    rm -f "$INSTALL_DIR/treemcp"
    echo "==> Removed binary: $INSTALL_DIR/treemcp"
fi

# 3. Clean any temp/cache files
rm -rf "$HOME/.cache/treemcp" "/tmp/treemcp_trash" 2>/dev/null || true

echo ""
echo "[SUCCESS] TreeMCP completely uninstalled!"

