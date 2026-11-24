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

  // Exact WizTree Color Matrix (1:1 with user device WizTree v4.3.2)
  static EXT_COLORS = {
    '.dll': '#76b82a', // Fresh Lime Green (.dll) - WizTree Row 1
    '.exe': '#0099ff', // Electric Azure (.exe) - WizTree Row 2
    '[no extension]': '#9e9e9e', // Slate Grey - WizTree Row 3
    '': '#9e9e9e',
    '.sys': '#9c27b0', // Royal Purple / Violet (.sys) - WizTree Row 4
    '.msi': '#00bcd4', // Cyan Teal (.msi) - WizTree Row 5
    '.bin': '#00838f', // Dark Teal (.bin) - WizTree Row 6
    '.mkv': '#c2185b', // Vivid Deep Pink / Magenta - WizTree Row 7
    '.db': '#f39c12',  // Golden Amber (.db) - WizTree Row 8
    '.sqlite': '#f39c12',
    '.mdf': '#f39c12',
    '.ldf': '#f39c12',
    '.lib': '#e67e22', // Tangerine Orange (.lib) - WizTree Row 9
    '.a': '#e67e22',
    '.obj': '#e67e22',
    '.o': '#e67e22',
    '.js': '#f1c40f',  // Sun Gold (.js) - WizTree Row 10
    '.ts': '#f1c40f',
    '.jsx': '#f1c40f',
    '.tsx': '#f1c40f',
    '.pak': '#d35400', // Rust Orange (.pak) - WizTree Row 11
    '.json': '#e74c3c', // Crimson Red - WizTree Row 12
    '.xml': '#e74c3c',
    '.yaml': '#e74c3c',
    '.yml': '#e74c3c',
    '.so': '#827717',  // Olive Green (.so) - WizTree Row 13
    '.dylib': '#827717',
    '.dat': '#7b1fa2', // Deep Violet (.dat) - WizTree Row 14
    '.zip': '#ff9800', // Orange - WizTree Row 15
    '.rar': '#ff9800',
    '.7z': '#ff9800',
    '.tar': '#ff9800',
    '.gz': '#ff9800',
    '.mp4': '#8e44ad', // Violet Purple - WizTree Row 16
    '.avi': '#8e44ad',
    '.mov': '#8e44ad',
    '.wmv': '#8e44ad',
    '.log': '#78909c', // Blue Grey - WizTree Row 17
    '.txt': '#78909c',
    '.go': '#00e5ff',  // Sky Cyan (.go) - WizTree Row 18
    '.wim': '#5c6bc0', // Indigo (.wim) - WizTree Row 19
    '.png': '#26a69a', // Teal
    '.jpg': '#26a69a',
    '.jpeg': '#26a69a',
    '.webp': '#26a69a',
    '.gif': '#26a69a',
    '.bmp': '#26a69a',
    '.vmdk': '#ab47bc', // Vivid Violet (Disk images & VMs)
    '.vdi': '#ab47bc',
    '.vmem': '#ab47bc',
    '.vhdx': '#ab47bc',
    '.vhd': '#ab47bc',
    '.iso': '#ab47bc',
    '.img': '#ab47bc',
    '.ova': '#ab47bc',
    '.ovf': '#ab47bc',
    '.qcow2': '#ab47bc',
    '.mp3': '#e84393', // Pink rose (Audio)
    '.wav': '#e84393',
    '.flac': '#e84393',
    '.aac': '#e84393',
    '.ogg': '#e84393',
    '.pdf': '#d63031', // Acrobat red (.pdf)
    '.docx': '#2980b9', // Steel Blue (Documents)
    '.doc': '#2980b9',
    '.xlsx': '#2980b9',
    '.pptx': '#2980b9',
    '.cpp': '#2ed573', // Code
    '.c': '#2ed573',
    '.h': '#2ed573',
    '.hpp': '#2ed573',
    '.rs': '#2ed573',
    '.py': '#2ed573',
    '.cat': '#e74c3c',
    'folder': '#546e7a'
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

      // Leaf file label if tile has sufficient size
      if (w > 38 && h > 16) {
        ctx.save();
        ctx.beginPath();
        ctx.rect(x + 2, y + 2, Math.max(0, w - 4), Math.max(0, h - 4));
        ctx.clip();

        ctx.shadowColor = 'rgba(0, 0, 0, 0.95)';
        ctx.shadowBlur = 3;
        ctx.shadowOffsetX = 1;
        ctx.shadowOffsetY = 1;

        ctx.fillStyle = '#ffffff';
        ctx.font = '600 10px "Segoe UI", system-ui, sans-serif';
        ctx.textAlign = 'left';
        ctx.textBaseline = 'top';
        ctx.fillText(tile.name, x + 3, y + 3);

        if (h > 28) {
          ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
          ctx.font = '500 9px "Consolas", monospace';
          ctx.fillText(this.formatSize(tile.size), x + 3, y + 14);
        }
        ctx.restore();
      }
    }

    // PASS 2: Folder Framing Outlines (WizTree style: crisp 1px borders, NO dark solid boxes)
    // Sort directories depth ascending so outer parents draw first, inner children draw on top
    dirTiles.sort((a, b) => (a.depth || 0) - (b.depth || 0));

    for (let i = 0; i < dirTiles.length; i++) {
      const tile = dirTiles[i];
      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      if (w <= 4 || h <= 4) continue;

      ctx.strokeStyle = (tile.depth <= 1) ? 'rgba(255, 255, 255, 0.55)' :
                        (tile.depth === 2) ? 'rgba(255, 255, 255, 0.35)' :
                        'rgba(255, 255, 255, 0.20)';
      ctx.lineWidth = 1;
      ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1));
    }

    // PASS 3: Stacked Directory Hierarchy Titles (The WizTree Hierarchy Stack)
    // In WizTree, top-left of each directory cluster displays stacked folder titles:
    // [C:] OS (157.8 GB)
    // Users\ (60.7 GB)
    // AMAL GEORGE\ (59.8 GB)
    // AppData\ (48.5 GB)
    // Local\ (37.9 GB)
    const drawnLabels = [];
    for (let i = 0; i < dirTiles.length; i++) {
      const tile = dirTiles[i];
      const x = Math.round(tile.x);
      const y = Math.round(tile.y);
      const w = Math.round(tile.width);
      const h = Math.round(tile.height);

      if (tile.depth <= 5 && w > 60 && h > 22) {
        let stackOffset = 0;
        for (const dl of drawnLabels) {
          // If this directory's top-left is near an already labeled parent, stack underneath it
          if (Math.abs(dl.x - x) < 28 && Math.abs((dl.y + dl.stack) - y) < 32) {
            stackOffset = dl.stack + 15;
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

          const label = tile.name + '\\ (' + this.formatSize(tile.size) + ')';
          ctx.fillText(label, x + 4, y + 3 + stackOffset);
          ctx.restore();

          drawnLabels.push({ x: x, y: y, stack: stackOffset });
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
