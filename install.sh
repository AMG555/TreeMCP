#!/usr/bin/env bash
# TreeMCP One-Click Installer for Linux
set -euo pipefail

INSTALL_DIR="$HOME/.local/bin"
mkdir -p "$INSTALL_DIR"

echo "==> Cloning and building TreeMCP..."
TMP_DIR=$(mktemp -d)
git clone --depth 1 https://github.com/AMG555/TreeMCP.git "$TMP_DIR"
make -C "$TMP_DIR"

mv "$TMP_DIR/treemcp" "$INSTALL_DIR/treemcp"
chmod +x "$INSTALL_DIR/treemcp"
rm -rf "$TMP_DIR"

echo "==> TreeMCP installed to: $INSTALL_DIR/treemcp"

# Configure Claude Desktop / MCP client config if present
CLAUDE_CONFIG="$HOME/.config/Claude/claude_desktop_config.json"
if [ -f "$CLAUDE_CONFIG" ]; then
    echo "==> Updating $CLAUDE_CONFIG..."
    TMP_JSON=$(mktemp)
    if command -v jq >/dev/null 2>&1; then
        jq --arg cmd "$INSTALL_DIR/treemcp" '.mcpServers.treemcp = {"command": $cmd, "args": ["--mcp"]}' "$CLAUDE_CONFIG" > "$TMP_JSON" && mv "$TMP_JSON" "$CLAUDE_CONFIG"
        echo "==> Registered treemcp in Claude Desktop config."
    else
        echo "==> Notice: Install jq to auto-update $CLAUDE_CONFIG"
    fi
fi

echo ""
echo "[SUCCESS] TreeMCP installed successfully!"
echo "Run GUI: $INSTALL_DIR/treemcp"
echo "Run MCP: $INSTALL_DIR/treemcp --mcp"
