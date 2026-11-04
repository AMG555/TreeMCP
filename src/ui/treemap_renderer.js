/**
 * TreeMCP - Interactive Canvas Treemap Renderer
 * Cushioned squarified rectangular layout with extension color mapping
 */

class TreemapRenderer {
  constructor(canvas, onTileClick, onTileHover) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.tiles = [];
    this.hoveredTile = null;
    this.selectedTile = null;
    this.onTileClick = onTileClick;
    this.onTileHover = onTileHover;

    this.initEvents();
  }

  setTiles(tiles) {
    this.tiles = tiles || [];
    this.render();
  }

  resize() {
    const rect = this.canvas.parentElement.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    this.canvas.width = rect.width * dpr;
    this.canvas.height = rect.height * dpr;
    this.ctx.resetTransform();
    this.ctx.scale(dpr, dpr);
    this.width = rect.width;
    this.height = rect.height;
    this.render();
  }

  initEvents() {
    window.addEventListener('resize', () => this.resize());

    this.canvas.addEventListener('mousemove', (e) => {
      const rect = this.canvas.getBoundingClientRect();
      const px = e.clientX - rect.left;
      const py = e.clientY - rect.top;

      const hit = this.hitTest(px, py);
      if (hit !== this.hoveredTile) {
        this.hoveredTile = hit;
        this.render();
        if (this.onTileHover) {
          this.onTileHover(hit, e.clientX, e.clientY);
        }
      } else if (hit && this.onTileHover) {
        this.onTileHover(hit, e.clientX, e.clientY);
      }
    });

    this.canvas.addEventListener('mouseleave', () => {
      if (this.hoveredTile) {
        this.hoveredTile = null;
        this.render();
        if (this.onTileHover) {
          this.onTileHover(null);
        }
      }
    });

    this.canvas.addEventListener('click', (e) => {
      const rect = this.canvas.getBoundingClientRect();
      const px = e.clientX - rect.left;
      const py = e.clientY - rect.top;

      const hit = this.hitTest(px, py);
      if (hit) {
        this.selectedTile = hit;
        this.render();
        if (this.onTileClick) {
          this.onTileClick(hit, false);
        }
      }
    });

    this.canvas.addEventListener('dblclick', (e) => {
      const rect = this.canvas.getBoundingClientRect();
      const px = e.clientX - rect.left;
      const py = e.clientY - rect.top;

      const hit = this.hitTest(px, py);
      if (hit && this.onTileClick) {
        this.onTileClick(hit, true); // true = double click zoom
      }
    });
  }

  hitTest(px, py) {
    let best = null;
    let maxDepth = -1;

    for (const t of this.tiles) {
      if (px >= t.x && px <= (t.x + t.width) && py >= t.y && py <= (t.y + t.height)) {
        if (t.depth >= maxDepth) {
          maxDepth = t.depth;
          best = t;
        }
      }
    }
    return best;
  }

  render() {
    if (!this.ctx || !this.width || !this.height) return;

    this.ctx.fillStyle = '#121216';
    this.ctx.fillRect(0, 0, this.width, this.height);

    if (!this.tiles || this.tiles.length === 0) {
      this.ctx.fillStyle = '#555';
      this.ctx.font = '14px sans-serif';
      this.ctx.textAlign = 'center';
      this.ctx.fillText('No scan data. Select a drive and click Scan.', this.width / 2, this.height / 2);
      return;
    }

    // Render tiles
    for (const tile of this.tiles) {
      const isHovered = (this.hoveredTile === tile);
      const isSelected = (this.selectedTile === tile);

      // Cushion gradient effect with depth borders
      const grad = this.ctx.createLinearGradient(tile.x, tile.y, tile.x + tile.width, tile.y + tile.height);

      let baseColor = tile.color || '#34495e';
      if (tile.is_dir) {
        baseColor = '#1f242d';
      }

      grad.addColorStop(0, this.adjustBrightness(baseColor, 20));
      grad.addColorStop(0.5, baseColor);
      grad.addColorStop(1, this.adjustBrightness(baseColor, -25));

      this.ctx.fillStyle = grad;
      this.ctx.fillRect(tile.x, tile.y, tile.width, tile.height);

      // Tile Borders
      this.ctx.lineWidth = isSelected ? 2 : (isHovered ? 1.5 : 1);
      this.ctx.strokeStyle = isSelected ? '#ffffff' : (isHovered ? '#00d2d3' : 'rgba(0,0,0,0.4)');
      this.ctx.strokeRect(tile.x, tile.y, tile.width, tile.height);

      // Label inside tile if size allows
      if (tile.width > 55 && tile.height > 26) {
        this.ctx.save();
        this.ctx.beginPath();
        this.ctx.rect(tile.x + 2, tile.y + 2, tile.width - 4, tile.height - 4);
        this.ctx.clip();

        this.ctx.fillStyle = '#ffffff';
        this.ctx.font = '11px sans-serif';
        this.ctx.textAlign = 'left';
        this.ctx.textBaseline = 'top';
        this.ctx.fillText(tile.name, tile.x + 4, tile.y + 4);

        if (tile.height > 38) {
          this.ctx.fillStyle = 'rgba(255,255,255,0.7)';
          this.ctx.font = '10px monospace';
          this.ctx.fillText(this.formatSize(tile.size), tile.x + 4, tile.y + 18);
        }

        this.ctx.restore();
      }
    }
  }

  adjustBrightness(hex, percent) {
    if (!hex || hex[0] !== '#') return hex;
    let num = parseInt(hex.slice(1), 16);
    let r = (num >> 16) + percent;
    let g = ((num >> 8) & 0x00FF) + percent;
    let b = (num & 0x0000FF) + percent;

    r = Math.min(255, Math.max(0, r));
    g = Math.min(255, Math.max(0, g));
    b = Math.min(255, Math.max(0, b));

    return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1)}`;
  }

  formatSize(bytes) {
    if (bytes >= 1024 * 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024 * 1024)).toFixed(1) + ' TB';
    if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
    if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
  }
}

