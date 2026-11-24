/**
 * TreeMCP - Hardware-Accelerated 3D Cushion Treemap Renderer
 * Dual-buffer offscreen rasterization, van Wijk parabolic cushion lighting,
 * and canonical Bruls-Huizing-van Wijk squarified layout (100% canvas coverage).
 */

class TreemapRenderer {
  constructor(canvas, onTileClick, onTileHover) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.offscreenCanvas = document.createElement('canvas');
    this.offscreenCtx = this.offscreenCanvas.getContext('2d');

    this.items = [];
    this.tiles = [];
    this.hoveredTile = null;
    this.selectedTile = null;
    this.highlightedExt = null;
    this.onTileClick = onTileClick;
    this.onTileHover = onTileHover;
    this.width = 0;
    this.height = 0;
    this.dpr = window.devicePixelRatio || 1;

    this.initEvents();
  }

  // Extension Color Matrix
  static EXT_COLORS = {
    '.dll': '#b359b6', // Orchid / Medium Purple (.dll)
    '.exe': '#0078d7', // Electric / Windows Azure (.exe)
    '[no extension]': '#d4a017', // Warm Golden Amber / Ochre
    '': '#d4a017',
    '.sys': '#00c853', // Vivid Emerald / Lime Green (.sys)
    '.msi': '#00b4d8', // Sky Cyan (.msi)
    '.bin': '#d97706', // Golden Caramel / Amber (.bin)
    '.mkv': '#dc2626', // Crimson Red (.mkv)
    '.db': '#3b82f6',  // Royal / Sea Blue (.db)
    '.sqlite': '#3b82f6',
    '.mdf': '#3b82f6',
    '.ldf': '#3b82f6',
    '.lib': '#65a30d', // Olive Green (.lib)
    '.a': '#65a30d',
    '.obj': '#65a30d',
    '.o': '#65a30d',
    '.js': '#ea580c',  // Vivid Tangerine / Orange (.js)
    '.ts': '#ea580c',
    '.jsx': '#ea580c',
    '.tsx': '#ea580c',
    '.pak': '#854d0e', // Russet Brown (.pak)
    '.json': '#ec4899', // Pink Rose (.json)
    '.xml': '#ec4899',
    '.yaml': '#ec4899',
    '.yml': '#ec4899',
    '.so': '#4338ca',  // Deep Indigo (.so)
    '.dylib': '#4338ca',
    '.dat': '#0d9488', // Deep Teal (.dat)
    '.zip': '#4d7c0f', // Forest Olive
    '.rar': '#4d7c0f',
    '.7z': '#4d7c0f',
    '.tar': '#4d7c0f',
    '.gz': '#4d7c0f',
    '.mp4': '#c026d3', // Vivid Magenta
    '.avi': '#c026d3',
    '.mov': '#c026d3',
    '.wmv': '#c026d3',
    '.log': '#ca8a04', // Sand / Khaki
    '.txt': '#ca8a04',
    '.go': '#06b6d4',  // Teal Cyan (.go)
    '.wim': '#64748b', // Slate Steel (.wim)
    '.png': '#f59e0b', // Warm Orange
    '.jpg': '#f59e0b',
    '.jpeg': '#f59e0b',
    '.webp': '#f59e0b',
    '.gif': '#f59e0b',
    '.bmp': '#f59e0b',
    '.vmdk': '#a855f7', // Vivid Violet (Disk images & VMs)
    '.vdi': '#a855f7',
    '.vmem': '#a855f7',
    '.vhdx': '#a855f7',
    '.vhd': '#a855f7',
    '.iso': '#a855f7',
    '.img': '#a855f7',
    '.ova': '#a855f7',
    '.ovf': '#a855f7',
    '.qcow2': '#a855f7',
    '.mp3': '#e11d48', // Pink rose (Audio)
    '.wav': '#e11d48',
    '.flac': '#e11d48',
    '.aac': '#e11d48',
    '.ogg': '#e11d48',
    '.pdf': '#ef4444', // Acrobat red (.pdf)
    '.docx': '#2563eb', // Steel Blue (Documents)
    '.doc': '#2563eb',
    '.xlsx': '#2563eb',
    '.pptx': '#2563eb',
    '.cpp': '#d97706', // Amber Code
    '.c': '#d97706',
    '.h': '#d97706',
    '.hpp': '#d97706',
    '.rs': '#d97706',
    '.py': '#d97706',
    '.cat': '#f43f5e',
    'folder': '#374151'
  };

  static DYNAMIC_PALETTE = [
    '#ff4757', '#2ed573', '#1e90ff', '#ffa502', '#a55eea',
    '#3742fa', '#70a1ff', '#7bed9f', '#eccc68', '#ff7f50',
    '#26de81', '#4b7bec', '#fed330', '#a55eea', '#fc5c65',
    '#fd9644', '#45aaf2', '#2bcbba', '#a55eea', '#778ca3',
    '#eb3b5a', '#fa8231', '#20bf6b', '#0fb9b1', '#3867d6',
    '#8854d0', '#d1d8e0', '#e17055', '#00cec9', '#6c5ce7',
    '#fdcb6e', '#00b894'
  ];

  getColorForExt(ext, isDir) {
    if (isDir) return TreemapRenderer.EXT_COLORS.folder;
    const cleanExt = (ext || '').toLowerCase();
    if (TreemapRenderer.EXT_COLORS[cleanExt]) {
      return TreemapRenderer.EXT_COLORS[cleanExt];
    }
    // Deterministic golden-ratio hash for unmapped extensions
    let hash = 2166136261;
    for (let i = 0; i < cleanExt.length; i++) {
      hash = (hash ^ cleanExt.charCodeAt(i)) * 16777619;
    }
    return TreemapRenderer.DYNAMIC_PALETTE[Math.abs(hash) % TreemapRenderer.DYNAMIC_PALETTE.length];
  }

  extractExt(name) {
    if (!name) return '';
    const idx = name.lastIndexOf('.');
    return idx >= 0 ? name.slice(idx).toLowerCase() : '';
  }

  setTiles(tilesOrItems) {
    if (!tilesOrItems || tilesOrItems.length === 0) {
      this.items = [];
      this.tiles = [];
      this.render();
      return;
    }

    const hasPrecomputed = tilesOrItems.length > 0 && typeof tilesOrItems[0].x === 'number' && typeof tilesOrItems[0].width === 'number';

    if (hasPrecomputed) {
      this.items = tilesOrItems;
      this.tiles = this.scaleLayoutToCanvas(tilesOrItems);
      this.renderOffscreen();
      this.render();
      return;
    }

    this.items = tilesOrItems.map(t => {
      const ext = t.extension || this.extractExt(t.name);
      return {
        node_idx: t.node_idx,
        name: t.name,
        full_path: t.full_path,
        size: t.size,
        is_dir: t.is_dir,
        extension: ext,
        color: t.color || this.getColorForExt(ext, t.is_dir)
      };
    });

    this.computeLayout();
    this.renderOffscreen();
    this.render();
  }

  // Scale precomputed hierarchical layout to current canvas dimensions
  scaleLayoutToCanvas(tiles) {
    if (!tiles || tiles.length === 0 || !this.width || !this.height) return [];

    let maxX = 0;
    let maxY = 0;
    for (let i = 0; i < tiles.length; i++) {
      const t = tiles[i];
      if (t.x + t.width > maxX) maxX = t.x + t.width;
      if (t.y + t.height > maxY) maxY = t.y + t.height;
    }

    if (maxX <= 0) maxX = 1000;
    if (maxY <= 0) maxY = 600;

    const scaleX = this.width / maxX;
    const scaleY = this.height / maxY;

    return tiles.map(t => {
      const ext = t.extension || this.extractExt(t.name);
      return {
        node_idx: t.node_idx,
        name: t.name,
        full_path: t.full_path,
        size: t.size,
        is_dir: t.is_dir,
        depth: t.depth || 0,
        extension: ext,
        color: t.color || this.getColorForExt(ext, t.is_dir),
        x: t.x * scaleX,
        y: t.y * scaleY,
        width: t.width * scaleX,
        height: t.height * scaleY
      };
    });
  }

  setHighlightedExtension(ext) {
    this.highlightedExt = ext ? ext.toLowerCase() : null;
    this.render();
  }

  resize() {
    const parent = this.canvas.parentElement;
    if (!parent) return;
    const rect = parent.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = Math.floor(rect.width);
    const h = Math.floor(rect.height);

    if (w <= 0 || h <= 0) return;

    this.dpr = dpr;
    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;

    this.offscreenCanvas.width = this.canvas.width;
    this.offscreenCanvas.height = this.canvas.height;

    this.ctx.resetTransform();
    this.ctx.scale(dpr, dpr);
    this.offscreenCtx.resetTransform();
    this.offscreenCtx.scale(dpr, dpr);

    this.width = w;
    this.height = h;

    if (this.items.length > 0 && typeof this.items[0].x === 'number') {
      this.tiles = this.scaleLayoutToCanvas(this.items);
    } else {
      this.computeLayout();
    }
    this.renderOffscreen();
    this.render();
  }

  // Canonical Squarified Treemap Layout (Bruls, Huizing, van Wijk)
  computeLayout() {
    if (!this.width || !this.height || this.items.length === 0) {
      this.tiles = [];
      return;
    }

    const sorted = [...this.items].sort((a, b) => b.size - a.size);
    const maxItems = Math.min(sorted.length, 800);
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
    if (side <= 0.001) return;

    const nextChild = children[0];

    if (row.length === 0) {
      this.squarify(children.slice(1), [nextChild], x, y, w, h, out);
      return;
    }

    const currentWorst = this.worstAspectRatio(row, side);
    const candidateRow = [...row, nextChild];
    const newWorst = this.worstAspectRatio(candidateRow, side);

    if (newWorst <= currentWorst) {
      this.squarify(children.slice(1), candidateRow, x, y, w, h, out);
    } else {
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

    if (totalRowArea <= 0 || minArea <= 0) return Infinity;
    const s2 = sideLength * sideLength;
    const a2 = totalRowArea * totalRowArea;
    return Math.max((s2 * maxArea) / a2, a2 / (s2 * minArea));
  }

  layoutRow(row, x, y, w, h, out) {
    let totalRowArea = 0;
    for (let i = 0; i < row.length; i++) {
      totalRowArea += row[i].area;
    }

    const isVertical = (w >= h);
    const side = isVertical ? h : w;
    const thickness = totalRowArea / side;

    let offset = 0;
    for (let i = 0; i < row.length; i++) {
      const it = row[i];
      const itemLen = (it.area / totalRowArea) * side;

      const tileX = isVertical ? x : (x + offset);
      const tileY = isVertical ? (y + offset) : y;
      const tileW = isVertical ? thickness : itemLen;
      const tileH = isVertical ? itemLen : thickness;

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

    if (isVertical) {
      return { x: x + thickness, y: y, w: Math.max(0, w - thickness), h: h };
    } else {
      return { x: x, y: y + thickness, w: w, h: Math.max(0, h - thickness) };
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
    let best = null;
    let maxDepth = -1;
    for (let i = 0; i < this.tiles.length; i++) {
      const t = this.tiles[i];
      if (px >= t.x && px < (t.x + t.width) && py >= t.y && py < (t.y + t.height)) {
        // Prioritize leaf files over directory container outlines
        const d = (t.depth || 0) + (t.is_dir ? 0 : 20);
        if (d >= maxDepth) {
          maxDepth = d;
          best = t;
        }
      }
    }
    return best;
  }

  // Pre-rasterize all tiles with true van Wijk 3D cushion lighting onto offscreen canvas
  renderOffscreen() {
    if (!this.offscreenCtx || !this.width || !this.height) return;

    const ctx = this.offscreenCtx;

    // Deep graphite canvas background
    ctx.fillStyle = '#18181b';
    ctx.fillRect(0, 0, this.width, this.height);

    if (!this.tiles || this.tiles.length === 0) {
      ctx.fillStyle = '#71717a';
      ctx.font = '12px "Segoe UI", system-ui, sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('Select a drive and click Scan to display treemap', this.width / 2, this.height / 2);
      return;
    }

    // Partition tiles into leaf cushions and directory grouping frames
    const fileTiles = [];
    const dirTiles = [];

    for (let i = 0; i < this.tiles.length; i++) {
      const t = this.tiles[i];
      if (t.is_dir) {
        dirTiles.push(t);
      } else {
        fileTiles.push(t);
      }
    }

    // PASS 1: Render All Leaf File Cushions (van Wijk 3D parabolic specular lighting)
    for (let i = 0; i < fileTiles.length; i++) {
      const tile = fileTiles[i];
      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      if (w <= 0 || h <= 0) continue;

      const baseColor = tile.color || this.getColorForExt(tile.extension, false);

      // Solid base fill
      ctx.fillStyle = baseColor;
      ctx.fillRect(x, y, w, h);

      // van Wijk 3D Parabolic Curved Cushion Lighting
      if (w > 3 && h > 3) {
        const cx = x + (w * 0.35);
        const cy = y + (h * 0.35);
        const rad = Math.max(w, h) * 0.95;

        const radial = ctx.createRadialGradient(cx, cy, 1, cx, cy, rad);
        radial.addColorStop(0, 'rgba(255, 255, 255, 0.28)');
        radial.addColorStop(0.30, 'rgba(255, 255, 255, 0.08)');
        radial.addColorStop(0.70, 'rgba(0, 0, 0, 0.10)');
        radial.addColorStop(1, 'rgba(0, 0, 0, 0.42)');

        ctx.fillStyle = radial;
        ctx.fillRect(x, y, w, h);

        // Specular Top/Left 1px Inner Bevel
        ctx.fillStyle = 'rgba(255, 255, 255, 0.35)';
        ctx.fillRect(x, y, w, 1);
        ctx.fillRect(x, y, 1, h);

        // Shadow Bottom/Right 1px Inner Bevel
        ctx.fillStyle = 'rgba(0, 0, 0, 0.40)';
        ctx.fillRect(x, y + h - 1, w, 1);
        ctx.fillRect(x + w - 1, y, 1, h);
      }

      // Crisp 1px black separation boundary between adjacent files
      ctx.strokeStyle = '#000000';
      ctx.lineWidth = 1;
      ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1));

      // Leaf file label ONLY if tile has sufficient size to prevent visual congestion
      if (w >= 100 && h >= 34) {
        ctx.save();
        ctx.beginPath();
        ctx.rect(x + 2, y + 2, Math.max(0, w - 4), Math.max(0, h - 4));
        ctx.clip();

        ctx.shadowColor = 'rgba(0, 0, 0, 0.95)';
        ctx.shadowBlur = 3;
        ctx.shadowOffsetX = 1;
        ctx.shadowOffsetY = 1;

        ctx.fillStyle = '#ffffff';
        ctx.font = '600 10.5px "Segoe UI", system-ui, sans-serif';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';

        if (h >= 46) {
          ctx.fillText(tile.name, x + 4, y + 4);
          ctx.fillStyle = 'rgba(255, 255, 255, 0.85)';
          ctx.font = '500 9.5px "Consolas", monospace';
          ctx.fillText(this.formatSize(tile.size), x + 4, y + 17);
        } else {
          ctx.fillText(tile.name + ' (' + this.formatSize(tile.size) + ')', x + 4, y + 4);
        }
        ctx.restore();
      }
    }

    // PASS 2: Folder Framing Outlines (crisp 1px borders, only for major levels)
    dirTiles.sort((a, b) => (a.depth || 0) - (b.depth || 0));

    for (let i = 0; i < dirTiles.length; i++) {
      const tile = dirTiles[i];
      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      if (tile.depth > 3 || w < 30 || h < 30) continue;

      ctx.strokeStyle = (tile.depth <= 1) ? 'rgba(255, 255, 255, 0.45)' :
                        (tile.depth === 2) ? 'rgba(255, 255, 255, 0.25)' :
                        'rgba(255, 255, 255, 0.15)';
      ctx.lineWidth = 1;
      ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1));
    }

    // PASS 3: Stacked Directory Hierarchy Titles
    // Only primary root branch gets stacked at top-left, plus major section headers
    const drawnLabels = [];
    for (let i = 0; i < dirTiles.length; i++) {
      const tile = dirTiles[i];
      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      const isTopLeftOrigin = (x <= 35 && y <= 35 && tile.depth <= 5);
      const isMajorSection = (tile.depth <= 2 && w >= 110 && h >= 45);

      if (isTopLeftOrigin || isMajorSection) {
        let stackOffset = 0;
        if (isTopLeftOrigin) {
          for (const dl of drawnLabels) {
            if (dl.isTopLeft) {
              stackOffset = Math.max(stackOffset, dl.stack + 15);
            }
          }
        }

        if (y + stackOffset + 14 < y + h) {
          ctx.save();
          ctx.shadowColor = 'rgba(0, 0, 0, 0.95)';
          ctx.shadowBlur = 4;
          ctx.shadowOffsetX = 1;
          ctx.shadowOffsetY = 1;
          ctx.fillStyle = '#ffffff';
          ctx.font = '700 10.5px "Segoe UI", system-ui, sans-serif';
          ctx.textAlign = 'left';
          ctx.textBaseline = 'top';

          const label = tile.name + ' (' + this.formatSize(tile.size) + ')';
          ctx.fillText(label, x + 4, y + 3 + stackOffset);
          ctx.restore();

          drawnLabels.push({ x: x, y: y, stack: stackOffset, isTopLeft: isTopLeftOrigin });
        }
      }
    }
  }

  // Fast 120 FPS Interactive Render: Draws pre-rasterized buffer + instant overlays
  render() {
    if (!this.ctx || !this.width || !this.height) return;

    // Fast bit-blit from offscreen canvas
    this.ctx.drawImage(this.offscreenCanvas, 0, 0);

    // If an extension filter is active, dim non-matching tiles
    if (this.highlightedExt && this.tiles && this.tiles.length > 0) {
      const len = this.tiles.length;
      for (let i = 0; i < len; i++) {
        const tile = this.tiles[i];
        const match = (tile.extension && tile.extension.toLowerCase() === this.highlightedExt);
        if (!match) {
          this.ctx.fillStyle = 'rgba(15, 15, 18, 0.65)';
          this.ctx.fillRect(Math.round(tile.x), Math.round(tile.y), Math.round(tile.width), Math.round(tile.height));
        } else {
          // Highlight border around matching extension tile
          this.ctx.strokeStyle = '#facc15';
          this.ctx.lineWidth = 1.5;
          this.ctx.strokeRect(Math.round(tile.x) + 0.5, Math.round(tile.y) + 0.5, Math.round(tile.width) - 1, Math.round(tile.height) - 1);
        }
      }
    }

    // Selected Tile Highlight (Crisp White 2px border)
    if (this.selectedTile) {
      const sx = Math.round(this.selectedTile.x);
      const sy = Math.round(this.selectedTile.y);
      const sw = Math.round(this.selectedTile.width);
      const sh = Math.round(this.selectedTile.height);
      this.ctx.lineWidth = 2.5;
      this.ctx.strokeStyle = '#ffffff';
      this.ctx.strokeRect(sx + 1, sy + 1, Math.max(1, sw - 2), Math.max(1, sh - 2));
    }

    // Hovered Tile Highlight (Cyan/Sky 1.5px border)
    if (this.hoveredTile && this.hoveredTile !== this.selectedTile) {
      const hx = Math.round(this.hoveredTile.x);
      const hy = Math.round(this.hoveredTile.y);
      const hw = Math.round(this.hoveredTile.width);
      const hh = Math.round(this.hoveredTile.height);
      this.ctx.lineWidth = 1.5;
      this.ctx.strokeStyle = '#38bdf8';
      this.ctx.strokeRect(hx + 0.5, hy + 0.5, Math.max(1, hw - 1), Math.max(1, hh - 1));
    }
  }

  formatSize(bytes) {
    if (bytes >= 1024 * 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024 * 1024)).toFixed(1) + ' TB';
    if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(1) + ' GB';
    if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
  }
}
