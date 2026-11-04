#!/usr/bin/env bash
# TreeMCP One-Click Installer for Linux
set -euo pipefail

INSTALL_DIR="$HOME/.local/bin"
DESKTOP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"

mkdir -p "$INSTALL_DIR" "$DESKTOP_DIR" "$ICON_DIR"

echo "==> Cloning and building TreeMCP..."
TMP_DIR=$(mktemp -d)
git clone --depth 1 https://github.com/AMG555/TreeMCP.git "$TMP_DIR"
make -C "$TMP_DIR"

mv "$TMP_DIR/treemcp" "$INSTALL_DIR/treemcp"
chmod +x "$INSTALL_DIR/treemcp"

# Install Desktop file and icon for system app launcher
if [ -f "$TMP_DIR/treemcp.desktop" ]; then
    cp "$TMP_DIR/treemcp.desktop" "$DESKTOP_DIR/treemcp.desktop"
    chmod +x "$DESKTOP_DIR/treemcp.desktop"
fi

if [ -f "$TMP_DIR/assets/logo.svg" ]; then
    cp "$TMP_DIR/assets/logo.svg" "$ICON_DIR/treemcp.svg"
fi

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
