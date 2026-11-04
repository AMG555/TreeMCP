/**
 * TreeMCP - High-Performance Canvas Treemap Renderer
 * Dynamic client-side squarified layout (100% canvas fill) with 3D cushion bevels.
 */

class TreemapRenderer {
  constructor(canvas, onTileClick, onTileHover) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.items = [];
    this.tiles = [];
    this.hoveredTile = null;
    this.selectedTile = null;
    this.onTileClick = onTileClick;
    this.onTileHover = onTileHover;
    this.width = 0;
    this.height = 0;

    this.initEvents();
  }

  // Accepts either pre-computed tiles or raw items and dynamically layouts to current viewport
  setTiles(tilesOrItems) {
    if (!tilesOrItems || tilesOrItems.length === 0) {
      this.items = [];
      this.tiles = [];
      this.render();
      return;
    }

    // Convert tiles into item list for dynamic squarification to ensure 100% canvas fill
    this.items = tilesOrItems.map(t => ({
      node_idx: t.node_idx,
      name: t.name,
      full_path: t.full_path,
      size: t.size,
      is_dir: t.is_dir,
      extension: t.extension || this.extractExt(t.name),
      color: t.color || this.getColorForExt(t.extension || this.extractExt(t.name), t.is_dir)
    }));

    this.computeLayout();
    this.render();
  }

  extractExt(name) {
    if (!name) return '';
    const idx = name.lastIndexOf('.');
    return idx >= 0 ? name.slice(idx).toLowerCase() : '';
  }

  getColorForExt(ext, isDir) {
    if (isDir) return '#242b35';
    switch (ext) {
      case '.mp4': case '.mkv': case '.avi': case '.mov': case '.wmv': case '.flv': case '.webm':
        return '#8b5cf6'; // Violet Video
      case '.zip': case '.rar': case '.7z': case '.tar': case '.gz': case '.bz2': case '.xz':
        return '#f59e0b'; // Amber Archive
      case '.exe': case '.dll': case '.sys': case '.msi': case '.bin': case '.drv':
        return '#ef4444'; // Crimson Binary/OS
      case '.iso': case '.vhd': case '.vhdx': case '.img': case '.vmdk':
        return '#ec4899'; // Pink VM/Disk Image
      case '.mp3': case '.wav': case '.flac': case '.m4a': case '.aac': case '.ogg':
        return '#10b981'; // Emerald Audio
      case '.jpg': case '.jpeg': case '.png': case '.gif': case '.webp': case '.bmp': case '.svg': case '.psd':
        return '#06b6d4'; // Cyan Image
      case '.pdf': case '.doc': case '.docx': case '.xls': case '.xlsx': case '.ppt': case '.pptx': case '.txt':
        return '#3b82f6'; // Cobalt Document
      case '.cpp': case '.h': case '.hpp': case '.c': case '.rs': case '.go': case '.py': case '.js': case '.ts': case '.json': case '.html': case '.css':
        return '#14b8a6'; // Teal Code
      default:
        return '#64748b'; // Slate Grey
    }
  }

  resize() {
    const parent = this.canvas.parentElement;
    if (!parent) return;
    const rect = parent.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.floor(rect.width);
    const h = Math.floor(rect.height);

    if (w <= 0 || h <= 0) return;

    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;

    this.ctx.resetTransform();
    this.ctx.scale(dpr, dpr);
    this.width = w;
    this.height = h;

    this.computeLayout();
    this.render();
  }

  // Pure JavaScript Squarified Layout (Bruls, Huizing, van Wijk)
  computeLayout() {
    if (!this.width || !this.height || this.items.length === 0) {
      this.tiles = [];
      return;
    }

    // Sort descending by size
    const sorted = [...this.items].sort((a, b) => b.size - a.size);

    // Keep top items to guarantee optimal 60fps interactive responsiveness
    const maxItems = Math.min(sorted.length, 450);
    const items = sorted.slice(0, maxItems);

    let totalSize = 0;
    for (let i = 0; i < items.length; i++) {
      totalSize += items[i].size;
    }
    if (totalSize === 0) {
      this.tiles = [];
      return;
    }

    const totalArea = this.width * this.height;
    const layoutItems = items.map(it => ({
      ...it,
      area: (it.size / totalSize) * totalArea
    }));

    const outTiles = [];
    this.squarify(layoutItems, [], 0, 0, this.width, this.height, outTiles);
    this.tiles = outTiles;
  }

  squarify(children, row, x, y, w, h, out) {
    if (children.length === 0) {
      if (row.length > 0) {
        this.layoutRow(row, x, y, w, h, out);
      }
      return;
    }

    const side = Math.min(w, h);
    const nextChild = children[0];

    if (row.length === 0) {
      this.squarify(children.slice(1), [nextChild], x, y, w, h, out);
      return;
    }

    const currentWorst = this.worstAspectRatio(row, side);
    const candidateRow = [...row, nextChild];
    const newWorst = this.worstAspectRatio(candidateRow, side);

    if (newWorst <= currentWorst) {
      // Adding improves or maintains aspect ratio
      this.squarify(children.slice(1), candidateRow, x, y, w, h, out);
    } else {
      // Row is full, lay it out and recurse into remaining space
      const remainingRect = this.layoutRow(row, x, y, w, h, out);
      this.squarify(children, [], remainingRect.x, remainingRect.y, remainingRect.w, remainingRect.h, out);
    }
  }

  worstAspectRatio(row, sideLength) {
    if (row.length === 0 || sideLength <= 0) return Infinity;
    let totalRowArea = 0;
    let minArea = Infinity;
    let maxArea = -Infinity;

    for (let i = 0; i < row.length; i++) {
      const a = row[i].area;
      totalRowArea += a;
      if (a < minArea) minArea = a;
      if (a > maxArea) maxArea = a;
    }

    if (totalRowArea <= 0) return Infinity;
    const s2 = sideLength * sideLength;
    const a2 = totalRowArea * totalRowArea;

    return Math.max((s2 * maxArea) / a2, a2 / (s2 * minArea));
  }

  layoutRow(row, x, y, w, h, out) {
    let totalRowArea = 0;
    for (let i = 0; i < row.length; i++) {
      totalRowArea += row[i].area;
    }

    const vertical = w >= h;
    const side = vertical ? h : w;
    const thickness = totalRowArea / side;

    let offset = 0;
    for (let i = 0; i < row.length; i++) {
      const it = row[i];
      const itemLen = (it.area / totalRowArea) * side;
      const tileX = vertical ? (x + offset) : x;
      const tileY = vertical ? y : (y + offset);
      const tileW = vertical ? itemLen : thickness;
      const tileH = vertical ? thickness : itemLen;

      out.push({
        node_idx: it.node_idx,
        name: it.name,
        full_path: it.full_path,
        size: it.size,
        is_dir: it.is_dir,
        extension: it.extension,
        color: it.color,
        x: tileX,
        y: tileY,
        width: tileW,
        height: tileH
      });

      offset += itemLen;
    }

    if (vertical) {
      return { x: x, y: y + thickness, w: w, h: Math.max(0, h - thickness) };
    } else {
      return { x: x + thickness, y: y, w: Math.max(0, w - thickness), h: h };
    }
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
        this.onTileClick(hit, true);
      }
    });
  }

  hitTest(px, py) {
    if (!this.tiles) return null;
    for (let i = this.tiles.length - 1; i >= 0; i--) {
      const t = this.tiles[i];
      if (px >= t.x && px < (t.x + t.width) && py >= t.y && py < (t.y + t.height)) {
        return t;
      }
    }
    return null;
  }

  render() {
    if (!this.ctx || !this.width || !this.height) return;

    // Background
    this.ctx.fillStyle = '#0d1117';
    this.ctx.fillRect(0, 0, this.width, this.height);

    if (!this.tiles || this.tiles.length === 0) {
      this.ctx.fillStyle = '#4b5563';
      this.ctx.font = '13px "Segoe UI", system-ui, sans-serif';
      this.ctx.textAlign = 'center';
      this.ctx.textBaseline = 'middle';
      this.ctx.fillText('Select a drive and click Scan to generate treemap', this.width / 2, this.height / 2);
      return;
    }

    // Render tiles with authentic 3D cushion beveling
    for (let i = 0; i < this.tiles.length; i++) {
      const tile = this.tiles[i];
      const isHovered = (this.hoveredTile === tile);
      const isSelected = (this.selectedTile === tile);

      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      if (w <= 1 || h <= 1) continue;

      const baseColor = tile.color || '#475569';

      // 3D Cushion Lighting
      const grad = this.ctx.createLinearGradient(x, y, x + w, y + h);
      grad.addColorStop(0, this.adjustBrightness(baseColor, 18));
      grad.addColorStop(0.45, baseColor);
      grad.addColorStop(1, this.adjustBrightness(baseColor, -24));

      this.ctx.fillStyle = grad;
      this.ctx.fillRect(x, y, w, h);

      // Inner Top/Left Highlight Bevel
      this.ctx.fillStyle = 'rgba(255, 255, 255, 0.16)';
      this.ctx.fillRect(x, y, w, 1);
      this.ctx.fillRect(x, y, 1, h);

      // Inner Bottom/Right Shadow Bevel
      this.ctx.fillStyle = 'rgba(0, 0, 0, 0.45)';
      this.ctx.fillRect(x, y + h - 1, w, 1);
      this.ctx.fillRect(x + w - 1, y, 1, h);

      // Highlight/Select Border
      if (isSelected) {
        this.ctx.lineWidth = 2;
        this.ctx.strokeStyle = '#ffffff';
        this.ctx.strokeRect(x + 1, y + 1, w - 2, h - 2);
      } else if (isHovered) {
        this.ctx.lineWidth = 1.5;
        this.ctx.strokeStyle = '#38bdf8';
        this.ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
      }

      // Tile Text Label (Clean drop shadow for high contrast readability)
      if (w > 52 && h > 22) {
        this.ctx.save();
        this.ctx.beginPath();
        this.ctx.rect(x + 2, y + 2, Math.max(0, w - 4), Math.max(0, h - 4));
        this.ctx.clip();

        this.ctx.shadowColor = 'rgba(0, 0, 0, 0.9)';
        this.ctx.shadowBlur = 3;
        this.ctx.shadowOffsetX = 1;
        this.ctx.shadowOffsetY = 1;

        this.ctx.fillStyle = '#ffffff';
        this.ctx.font = '600 11px "Segoe UI", system-ui, sans-serif';
        this.ctx.textAlign = 'left';
        this.ctx.textBaseline = 'top';
        this.ctx.fillText(tile.name, x + 4, y + 4);

        if (h > 36) {
          this.ctx.fillStyle = 'rgba(255, 255, 255, 0.85)';
          this.ctx.font = '500 10px "Cascadia Code", Consolas, monospace';
          this.ctx.fillText(this.formatSize(tile.size), x + 4, y + 18);
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
