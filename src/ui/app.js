/**
 * TreeMCP Main Application UI Controller
 * High-speed native desktop interactivity, synchronized treemap,
 * live extension legend strip, zero AI slop, zero phone emojis.
 */

let treemapRenderer = null;
let currentScanData = null;
let currentDrives = [];
let selectedNodeIdx = null;
let breadcrumbPath = [];
let expandedNodes = new Set();
let contextTarget = null;
let topHogsData = [];
let duplicatesData = [];
let activeLegendExt = null;

// Session token for anti-CSRF
const urlParams = new URLSearchParams(window.location.search);
const sessionToken = urlParams.get('token') || '';

// Secure Fetch Helper: Attaches X-TreeMCP-Token header
async function secureFetch(url, options = {}) {
  const headers = options.headers || {};
  if (sessionToken) {
    headers['X-TreeMCP-Token'] = sessionToken;
  }
  return fetch(url, { ...options, headers });
}

// Standard US thousands grouping (e.g. 1,249,600)
function formatNumber(num) {
  if (num === undefined || num === null) return '0';
  return Number(num).toLocaleString('en-US');
}

// Vector SVG Icons for Tree Nodes
const SVG_FOLDER = '<svg class="tree-glyph-svg" width="13" height="13" viewBox="0 0 24 24" fill="#eab308"><path d="M2 6a2 2 0 0 1 2-2h5l2 2h9a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6z"/></svg>';
const SVG_FILE = '<svg class="tree-glyph-svg" width="13" height="13" viewBox="0 0 24 24" fill="#94a3b8"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6zm2 16H8v-2h8v2zm0-4H8v-2h8v2zm-3-5V3.5L18.5 9H13z"/></svg>';

document.addEventListener('DOMContentLoaded', () => {
  const canvas = document.getElementById('treemap-canvas');
  treemapRenderer = new TreemapRenderer(canvas, handleTileClick, handleTileHover);

  setTimeout(() => treemapRenderer.resize(), 60);

  setupMenuBar();
  setupTabs();
  setupSplitter();
  setupMcpConfigCard();
  setupEventListeners();
  loadDrives();
  checkAdminStatus();
  startMcpEventStream();
});

// 1. Native Desktop Menu Bar Wiring
function setupMenuBar() {
  const triggerScan = () => {
    const customPath = document.getElementById('custom-path').value.trim();
    const driveSelect = document.getElementById('drive-select').value;
    startScan(customPath || driveSelect);
  };

  const bind = (id, fn) => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('click', fn);
  };

  bind('m-open-path', () => {
    const p = prompt('Enter directory path to scan:', document.getElementById('custom-path').value || 'C:\\');
    if (p) {
      document.getElementById('custom-path').value = p.trim();
      triggerScan();
    }
  });

  bind('m-rescan', triggerScan);
  bind('m-export-csv', () => {
    window.location.href = '/api/export-csv?token=' + encodeURIComponent(sessionToken);
  });
  bind('m-exit', async () => {
    try { await secureFetch('/api/exit'); } catch (e) {}
    window.close();
  });

  bind('m-scan-now', triggerScan);
  bind('m-elevate-admin', requestAdminElevation);
  bind('m-clear-cache', () => {
    currentScanData = null;
    treemapRenderer.setTiles([]);
    document.getElementById('tree-table-body').innerHTML = '<tr><td colspan="6" class="table-placeholder">Scan cleared.</td></tr>';
    document.getElementById('legend-bar').innerHTML = '<span class="legend-empty-text">Scan a drive to view file type legend</span>';
  });

  bind('m-view-tree', () => switchTab('pane-tree'));
  bind('m-view-hogs', () => switchTab('pane-top-hogs'));
  bind('m-view-dups', () => switchTab('pane-duplicates'));
  bind('m-view-ext', () => switchTab('pane-ext'));
  bind('m-view-mcp', () => switchTab('pane-mcp'));

  bind('m-reset-panels', () => {
    const upperPane = document.getElementById('pane-upper');
    const viewport = document.getElementById('viewport');
    upperPane.style.height = `${Math.floor(viewport.clientHeight * 0.5)}px`;
    treemapRenderer.resize();
  });

  bind('m-tool-recycle', () => {
    if (contextTarget) document.getElementById('ctx-safe-recycle').click();
  });
  bind('m-tool-explorer', () => {
    if (contextTarget && contextTarget.full_path) {
      secureFetch('/api/open-explorer?path=' + encodeURIComponent(contextTarget.full_path));
    }
  });
  bind('m-tool-copy', () => {
    if (contextTarget && contextTarget.full_path) {
      navigator.clipboard.writeText(contextTarget.full_path);
    }
  });
  bind('m-tool-inspect', () => {
    if (contextTarget) {
      switchTab('pane-mcp');
      logMcpAction(`Inspected: ${contextTarget.full_path} (${treemapRenderer.formatSize(contextTarget.size)})`);
    }
  });

  bind('m-help-github', () => {
    window.open('https://github.com/AMG555/TreeMCP', '_blank');
  });
  bind('m-help-mcp-spec', () => {
    window.open('https://modelcontextprotocol.io', '_blank');
  });
  bind('m-help-about', () => {
    document.getElementById('modal-about').style.display = 'flex';
  });
}

function switchTab(targetId) {
  document.querySelectorAll('.d-tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.target === targetId);
  });
  document.querySelectorAll('.tab-content-pane').forEach(p => {
    p.classList.toggle('active', p.id === targetId);
  });

  if (targetId === 'pane-top-hogs' && topHogsData.length === 0) loadTopHogs();
  if (targetId === 'pane-duplicates' && duplicatesData.length === 0) loadDuplicates();
}

// 2. Segmented Desktop Tab Switching
function setupTabs() {
  const tabs = document.querySelectorAll('.d-tab-btn');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      switchTab(tab.dataset.target);
    });
  });
}

// 3. Draggable Horizontal Splitter
function setupSplitter() {
  const splitter = document.getElementById('splitter');
  const upperPane = document.getElementById('pane-upper');
  const viewport = document.getElementById('viewport');

  let isDragging = false;
  let startY = 0;
  let startHeight = 0;

  splitter.addEventListener('mousedown', (e) => {
    isDragging = true;
    startY = e.clientY;
    startHeight = upperPane.getBoundingClientRect().height;
    splitter.classList.add('dragging');
    document.body.style.cursor = 'row-resize';
    e.preventDefault();
  });

  window.addEventListener('mousemove', (e) => {
    if (!isDragging) return;
    const dy = e.clientY - startY;
    const viewportHeight = viewport.getBoundingClientRect().height;
    const newHeight = Math.max(100, Math.min(viewportHeight - 100, startHeight + dy));
    upperPane.style.height = `${newHeight}px`;
    treemapRenderer.resize();
  });

  window.addEventListener('mouseup', () => {
    if (isDragging) {
      isDragging = false;
      splitter.classList.remove('dragging');
      document.body.style.cursor = '';
      treemapRenderer.resize();
    }
  });

  // Vertical Dual-Tree Splitter
  const dualSplitter = document.getElementById('dual-splitter');
  const dualLeft = document.querySelector('.dual-tree-left');
  const dualWrapper = document.querySelector('.dual-tree-wrapper');

  if (dualSplitter && dualLeft && dualWrapper) {
    let isDualDragging = false;
    let startX = 0;
    let startW = 0;

    dualSplitter.addEventListener('mousedown', (e) => {
      isDualDragging = true;
      startX = e.clientX;
      startW = dualLeft.getBoundingClientRect().width;
      dualSplitter.classList.add('dragging');
      document.body.style.cursor = 'col-resize';
      e.preventDefault();
    });

    window.addEventListener('mousemove', (e) => {
      if (!isDualDragging) return;
      const dx = e.clientX - startX;
      const totalW = dualWrapper.getBoundingClientRect().width;
      const newW = Math.max(200, Math.min(totalW - 200, startW + dx));
      dualLeft.style.flex = `0 0 ${newW}px`;
    });

    window.addEventListener('mouseup', () => {
      if (isDualDragging) {
        isDualDragging = false;
        dualSplitter.classList.remove('dragging');
        document.body.style.cursor = '';
      }
    });
  }
}

// 4. MCP Config Generator Card
function setupMcpConfigCard() {
  const tabs = document.querySelectorAll('.conn-tab-btn');
  const pathElem = document.getElementById('mcp-config-path');
  const snippetElem = document.getElementById('mcp-json-snippet');
  const copyBtn = document.getElementById('btn-copy-mcp');
  const copyBtnText = document.getElementById('copy-btn-text');

  const configs = {
    claude: {
      path: '%APPDATA%\\Claude\\claude_desktop_config.json',
      json: `{\n  "mcpServers": {\n    "treemcp": {\n      "command": "TreeMCP.exe",\n      "args": ["--mcp"]\n    }\n  }\n}`
    },
    cursor: {
      path: '%USERPROFILE%\\.cursor\\mcp.json',
      json: `{\n  "mcpServers": {\n    "treemcp": {\n      "command": "TreeMCP.exe",\n      "args": ["--mcp"]\n    }\n  }\n}`
    },
    antigravity: {
      path: '%USERPROFILE%\\.gemini\\antigravity\\mcp_config.json',
      json: `{\n  "mcpServers": {\n    "treemcp": {\n      "command": "TreeMCP.exe",\n      "args": ["--mcp"]\n    }\n  }\n}`
    },
    windsurf: {
      path: '%USERPROFILE%\\.codeium\\windsurf\\mcp_config.json',
      json: `{\n  "mcpServers": {\n    "treemcp": {\n      "command": "TreeMCP.exe",\n      "args": ["--mcp"]\n    }\n  }\n}`
    }
  };

  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');
      const client = tab.dataset.client;
      if (configs[client]) {
        pathElem.textContent = configs[client].path;
        snippetElem.textContent = configs[client].json;
      }
    });
  });

  if (copyBtn) {
    copyBtn.addEventListener('click', () => {
      const textToCopy = snippetElem.textContent;
      navigator.clipboard.writeText(textToCopy).then(() => {
        if (copyBtnText) {
          copyBtnText.textContent = 'Copied!';
          setTimeout(() => { copyBtnText.textContent = 'Copy JSON'; }, 1800);
        }
      });
    });
  }
}

// 5. Toolbar and Application Event Listeners
function setupEventListeners() {
  const triggerScan = () => {
    const customPath = document.getElementById('custom-path').value.trim();
    const driveSelect = document.getElementById('drive-select').value;
    startScan(customPath || driveSelect);
  };

  document.getElementById('btn-scan').addEventListener('click', triggerScan);

  const btnRefresh = document.getElementById('btn-refresh');
  if (btnRefresh) btnRefresh.addEventListener('click', triggerScan);

  const btnElevate = document.getElementById('btn-elevate');
  if (btnElevate) btnElevate.addEventListener('click', requestAdminElevation);

  const btnExportCsv = document.getElementById('btn-export-csv');
  if (btnExportCsv) {
    btnExportCsv.addEventListener('click', () => {
      window.location.href = '/api/export-csv?token=' + encodeURIComponent(sessionToken);
    });
  }

  document.getElementById('custom-path').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') triggerScan();
  });

  document.getElementById('drive-select').addEventListener('change', (e) => {
    document.getElementById('custom-path').value = e.target.value;
    const drv = currentDrives.find(d => d.path === e.target.value);
    if (drv) updateDriveUsage(drv);
  });

  let filterTimeout = null;
  document.getElementById('search-filter').addEventListener('input', (e) => {
    clearTimeout(filterTimeout);
    filterTimeout = setTimeout(() => filterActiveTable(e.target.value), 80);
  });

  // Modal Dialogs
  const modalAbout = document.getElementById('modal-about');
  const btnAbout = document.getElementById('btn-about');
  const modalClose = document.getElementById('modal-close');
  const modalOk = document.getElementById('modal-ok-btn');

  if (btnAbout && modalAbout) {
    btnAbout.addEventListener('click', () => modalAbout.style.display = 'flex');
    modalClose.addEventListener('click', () => modalAbout.style.display = 'none');
    modalOk.addEventListener('click', () => modalAbout.style.display = 'none');
    modalAbout.addEventListener('click', (e) => {
      if (e.target === modalAbout) modalAbout.style.display = 'none';
    });
  }

  // Keyboard Shortcuts
  window.addEventListener('keydown', (e) => {
    if (e.key === 'F5' || (e.ctrlKey && e.key === 'r')) {
      e.preventDefault();
      triggerScan();
    } else if (e.ctrlKey && e.key === 'f') {
      e.preventDefault();
      document.getElementById('search-filter').focus();
    } else if (e.ctrlKey && e.key === 'd') {
      e.preventDefault();
      document.getElementById('drive-select').focus();
    } else if (e.ctrlKey && e.shiftKey && (e.key === 'A' || e.key === 'a')) {
      e.preventDefault();
      requestAdminElevation();
    } else if (e.key === 'Escape') {
      if (modalAbout) modalAbout.style.display = 'none';
      document.getElementById('context-menu').style.display = 'none';
    }
  });

  // Context Menu Close
  window.addEventListener('click', () => {
    document.getElementById('context-menu').style.display = 'none';
  });

  // Treemap Context Menu
  document.getElementById('treemap-canvas').addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const rect = e.target.getBoundingClientRect();
    const hit = treemapRenderer.hitTest(e.clientX - rect.left, e.clientY - rect.top);
    if (hit) {
      contextTarget = hit;
      showContextMenu(e.clientX, e.clientY);
    }
  });

  // Table Row Context Menu delegation
  document.getElementById('tree-table-body').addEventListener('contextmenu', (e) => {
    const tr = e.target.closest('tr');
    if (!tr || !tr.dataset.fullPath) return;
    e.preventDefault();
    contextTarget = {
      name: tr.querySelector('.tree-name')?.textContent || '',
      full_path: tr.dataset.fullPath,
      size: Number(tr.dataset.size || 0),
      is_dir: tr.dataset.isDir === 'true'
    };
    showContextMenu(e.clientX, e.clientY);
  });

  document.getElementById('hogs-table-body').addEventListener('contextmenu', (e) => {
    const tr = e.target.closest('tr');
    if (!tr || !tr.dataset.fullPath) return;
    e.preventDefault();
    contextTarget = {
      name: tr.dataset.name || '',
      full_path: tr.dataset.fullPath,
      size: Number(tr.dataset.size || 0),
      is_dir: false
    };
    showContextMenu(e.clientX, e.clientY);
  });

  // Context Menu Actions
  document.getElementById('ctx-copy-path').addEventListener('click', () => {
    if (contextTarget && contextTarget.full_path) {
      navigator.clipboard.writeText(contextTarget.full_path);
    }
  });

  document.getElementById('ctx-open-explorer').addEventListener('click', () => {
    if (contextTarget && contextTarget.full_path) {
      secureFetch('/api/open-explorer?path=' + encodeURIComponent(contextTarget.full_path));
    }
  });

  document.getElementById('ctx-mcp-inspect').addEventListener('click', () => {
    if (contextTarget) {
      switchTab('pane-mcp');
      logMcpAction(`Inspected item: ${contextTarget.full_path} (${treemapRenderer.formatSize(contextTarget.size)})`);
    }
  });

  document.getElementById('ctx-safe-recycle').addEventListener('click', async () => {
    if (!contextTarget || !contextTarget.full_path) return;
    const targetPath = contextTarget.full_path;
    if (confirm(`Move "${targetPath}" safely to the Recycle Bin / Trash?\n\n(Protected system paths are automatically blocked).`)) {
      try {
        const res = await secureFetch('/api/clean?path=' + encodeURIComponent(targetPath) + '&dry_run=false');
        const data = await res.json();
        if (data.success) {
          alert('Item successfully moved to Recycle Bin.');
          triggerScan();
        } else {
          alert('Operation blocked: ' + (data.message || 'Protected system file or access denied.'));
        }
      } catch (err) {
        alert('Backend communication error: ' + err.message);
      }
    }
  });

  document.getElementById('ctx-properties').addEventListener('click', () => {
    if (!contextTarget) return;
    alert(`Properties for: ${contextTarget.name}\n\nFull Path: ${contextTarget.full_path}\nSize: ${treemapRenderer.formatSize(contextTarget.size)}\nType: ${contextTarget.is_dir ? 'Directory' : 'File'}`);
  });
}

async function requestAdminElevation() {
  try {
    const res = await secureFetch('/api/elevate');
    const data = await res.json().catch(() => ({ ok: true }));
    if (data.ok) {
      document.getElementById('status-text').textContent = 'Restarting with Administrator privileges...';
      // Close old non-elevated browser window cleanly as new elevated window opens
      setTimeout(() => {
        window.close();
      }, 800);
    } else {
      alert('Elevation could not be completed: ' + (data.message || 'Cancelled.'));
    }
  } catch (err) {
    // If connection dropped because old process yielded port, new elevated instance is opening
    document.getElementById('status-text').textContent = 'Restarting with Administrator privileges...';
    setTimeout(() => {
      window.close();
    }, 800);
  }
}

async function checkAdminStatus() {
  try {
    const res = await secureFetch('/api/system-info');
    if (!res.ok) return;
    const data = await res.json();

    const btnElevate = document.getElementById('btn-elevate');
    const engineTag = document.getElementById('status-engine-tag');

    if (data.is_admin) {
      if (btnElevate) btnElevate.style.display = 'none';
      if (engineTag) {
        engineTag.textContent = 'NTFS MFT Direct (Admin)';
        engineTag.className = 'status-tag tag-success';
      }
    } else {
      if (btnElevate) btnElevate.style.display = 'inline-flex';
      if (engineTag) {
        engineTag.textContent = 'Win32 Standard (Run as Admin for 1s MFT)';
        engineTag.className = 'status-tag';
      }
    }
  } catch (e) {}
}

async function loadDrives() {
  try {
    const res = await secureFetch('/api/drives');
    if (!res.ok) return;
    const data = await res.json();
    currentDrives = data.drives || [];

    const select = document.getElementById('drive-select');
    select.innerHTML = '';
    currentDrives.forEach(d => {
      const opt = document.createElement('option');
      opt.value = d.path;
      const totalGb = (d.total_bytes / (1024 ** 3)).toFixed(1);
      const freeGb = (d.free_bytes / (1024 ** 3)).toFixed(1);
      opt.textContent = `${d.path} [${d.volume_name || 'Drive'}] - ${freeGb} GB free of ${totalGb} GB (${d.file_system})`;
      select.appendChild(opt);
    });

    if (currentDrives.length > 0) {
      document.getElementById('custom-path').value = currentDrives[0].path;
      updateDriveUsage(currentDrives[0]);
    }
  } catch (e) {
    console.error('Failed to load drives:', e);
  }
}

function updateDriveUsage(drive) {
  if (!drive || !drive.total_bytes) return;
  const used = drive.total_bytes - drive.free_bytes;
  const pct = Math.round((used / drive.total_bytes) * 100);

  const fill = document.getElementById('capacity-fill');
  const label = document.getElementById('capacity-text');

  if (fill) {
    fill.style.width = `${pct}%`;
    if (pct >= 90) {
      fill.style.backgroundColor = 'var(--accent-red)';
    } else if (pct >= 80) {
      fill.style.backgroundColor = 'var(--accent-amber)';
    } else {
      fill.style.backgroundColor = 'var(--accent-blue)';
    }
  }

  if (label) {
    label.textContent = `${drive.path} ${pct}% used (${treemapRenderer.formatSize(used)} / ${treemapRenderer.formatSize(drive.total_bytes)})`;
  }
}

// 6. Start Scan Operation
async function startScan(path) {
  const btnScan = document.getElementById('btn-scan');
  btnScan.disabled = true;
  btnScan.innerHTML = '<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M12 2v4m0 12v4M4.93 4.93l2.83 2.83m8.48 8.48l2.83 2.83M2 12h4m12 0h4M4.93 19.07l2.83-2.83m8.48-8.48l2.83-2.83"/></svg><span>Scanning...</span>';

  document.getElementById('status-text').textContent = `Scanning ${path}...`;

  try {
    const res = await secureFetch(`/api/scan?path=${encodeURIComponent(path)}`);
    if (!res.ok) {
      const errData = await res.json().catch(() => ({}));
      document.getElementById('status-text').textContent = `Scan failed: ${errData.error || 'Access Denied'}`;
      return;
    }
    const data = await res.json();
    currentScanData = data;
    topHogsData = []; // Invalidate top hogs cache

    renderScanResults(data);
    const modeStr = data.stats.mft_used ? 'NTFS MFT Direct' : 'Win32 Walker';
    logMcpAction(`Scan completed: ${path} (${formatNumber(data.stats.total_files)} files in ${(data.stats.duration_ms / 1000).toFixed(2)}s via ${modeStr})`);

    // Pre-fetch Top 100 files in background
    loadTopHogs();
  } catch (err) {
    console.error('Scan failed:', err);
    document.getElementById('status-text').textContent = 'Scan error.';
  } finally {
    btnScan.disabled = false;
    btnScan.innerHTML = '<svg class="tb-btn-svg" width="12" height="12" viewBox="0 0 24 24" fill="currentColor"><path d="M8 5v14l11-7z"/></svg><span>Scan</span>';
  }
}

function renderScanResults(data) {
  if (!data) return;

  // Refresh drive capacity bar with live drive free space from current selected drive
  loadDrives();

  // Header quick metrics (Standard US thousands grouping)
  document.getElementById('stat-files').textContent = formatNumber(data.stats.total_files || 0);
  document.getElementById('stat-folders').textContent = formatNumber(data.stats.total_dirs || 0);
  document.getElementById('stat-size').textContent = treemapRenderer.formatSize(data.stats.total_bytes || 0);
  document.getElementById('stat-time').textContent = `${(data.stats.duration_ms / 1000).toFixed(2)}s`;

  const engineType = data.stats.mft_used ? 'NTFS MFT Direct' : 'Win32 Standard';
  document.getElementById('status-text').textContent = `Scan completed in ${(data.stats.duration_ms / 1000).toFixed(2)}s (${engineType})`;

  // Render Tree View
  renderTreeView(data.tree_root);

  // Render Extensions Table
  renderExtensionsTable(data.extensions || [], data.stats.total_bytes || 0);

  // Render Live Extension Legend Strip directly above treemap
  renderLegendBar(data.extensions || [], data.stats.total_bytes || 0);

  // Render Treemap Canvas (100% canvas coverage)
  treemapRenderer.setTiles(data.treemap_tiles || []);
  updateBreadcrumbs([data.tree_root ? data.tree_root.name : 'Root']);
}

// 7. Live Interactive Extension Legend Strip above Treemap
function renderLegendBar(extensions, totalBytes) {
  const bar = document.getElementById('legend-bar');
  bar.innerHTML = '';

  if (!extensions || extensions.length === 0) {
    bar.innerHTML = '<span class="legend-empty-text">No extension data</span>';
    return;
  }

  // Show top 8 extensions
  const topExts = extensions.slice(0, 8);
  activeLegendExt = null;

  topExts.forEach(ext => {
    const chip = document.createElement('div');
    chip.className = 'legend-chip';
    const pct = totalBytes > 0 ? ((ext.total_size / totalBytes) * 100).toFixed(1) : 0;
    const extName = ext.ext || '[none]';

    chip.innerHTML = `
      <span class="legend-swatch" style="background-color: ${ext.color};"></span>
      <span class="legend-ext">${escapeHtml(extName)}</span>
      <span class="legend-size">${treemapRenderer.formatSize(ext.total_size)}</span>
      <span class="legend-pct">${pct}%</span>
    `;

    chip.addEventListener('click', () => {
      if (activeLegendExt === extName) {
        // Toggle off
        activeLegendExt = null;
        chip.classList.remove('active');
        treemapRenderer.setHighlightedExtension(null);
      } else {
        // Toggle on
        document.querySelectorAll('.legend-chip').forEach(c => c.classList.remove('active'));
        activeLegendExt = extName;
        chip.classList.add('active');
        treemapRenderer.setHighlightedExtension(ext.ext);
      }
    });

    chip.addEventListener('mouseenter', () => {
      if (!activeLegendExt) {
        treemapRenderer.setHighlightedExtension(ext.ext);
      }
    });

    chip.addEventListener('mouseleave', () => {
      if (!activeLegendExt) {
        treemapRenderer.setHighlightedExtension(null);
      }
    });

    bar.appendChild(chip);
  });
}

function renderTreeView(root) {
  const tbody = document.getElementById('tree-table-body');
  tbody.innerHTML = '';
  if (!root) return;

  expandedNodes.clear();
  expandedNodes.add(root.node_idx);

  renderTreeNode(root, 0, tbody, root.size);
}

function renderTreeNode(node, depth, tbody, rootTotalSize) {
  const tr = document.createElement('tr');
  tr.dataset.nodeIdx = node.node_idx;
  tr.dataset.fullPath = node.full_path;
  tr.dataset.size = node.size;
  tr.dataset.isDir = node.is_dir;

  const pctOfParent = (rootTotalSize > 0) ? ((node.size / rootTotalSize) * 100).toFixed(1) : 0;
  const iconSvg = node.is_dir ? SVG_FOLDER : SVG_FILE;
  const hasChildren = node.has_children || (node.children && node.children.length > 0) || (node.is_dir && node.file_count + node.dir_count > 0);
  const isExpanded = expandedNodes.has(node.node_idx);
  const expanderChar = hasChildren ? (isExpanded ? '▼' : '▶') : '';

  tr.innerHTML = `
    <td>
      <div class="tree-node-wrap">
        <span class="tree-indent" style="width: ${depth * 14}px"></span>
        <span class="tree-expander">${expanderChar}</span>
        ${iconSvg}
        <span class="tree-name" title="${escapeHtml(node.full_path)}">${escapeHtml(node.name)}</span>
      </div>
    </td>
    <td>
      <div class="pct-cell-wrap">
        <div class="pct-track">
          <div class="pct-fill" style="width: ${pctOfParent}%"></div>
        </div>
        <span class="pct-val">${pctOfParent}%</span>
      </div>
    </td>
    <td class="col-size">${treemapRenderer.formatSize(node.size)}</td>
    <td class="col-alloc">${treemapRenderer.formatSize(node.allocated_size || node.size)}</td>
    <td class="col-num">${formatNumber(node.file_count || 0)}</td>
    <td class="col-num">${formatNumber(node.dir_count || 0)}</td>
  `;

  // Click row to select
  tr.addEventListener('click', () => {
    document.querySelectorAll('#tree-table-body tr').forEach(r => r.classList.remove('selected'));
    tr.classList.add('selected');
    selectedNodeIdx = node.node_idx;
    document.getElementById('status-selected').textContent = `${node.full_path} (${treemapRenderer.formatSize(node.size)})`;

    // Highlight tile in treemap
    const matchedTile = treemapRenderer.tiles.find(t => t.node_idx === node.node_idx);
    if (matchedTile) {
      treemapRenderer.selectedTile = matchedTile;
      treemapRenderer.render();
    }
  });

  // Double click directory to drill down in treemap
  tr.addEventListener('dblclick', () => {
    if (node.is_dir) {
      secureFetch(`/api/treemap?node_idx=${node.node_idx}`)
        .then(res => res.json())
        .then(data => {
          treemapRenderer.setTiles(data.tiles || []);
          breadcrumbPath.push({ name: node.name, node_idx: node.node_idx });
          updateBreadcrumbs();
        });
    }
  });

  // Toggle expansion
  const expander = tr.querySelector('.tree-expander');
  if (hasChildren && expander) {
    expander.addEventListener('click', async (e) => {
      e.stopPropagation();
      if (expandedNodes.has(node.node_idx)) {
        expandedNodes.delete(node.node_idx);
        collapseNodeRows(node.node_idx, tbody);
        expander.textContent = '▶';
      } else {
        expandedNodes.add(node.node_idx);
        expander.textContent = '▼';
        await expandNodeChildren(node, depth + 1, tr, tbody, rootTotalSize);
      }
    });
  }

  tbody.appendChild(tr);

  // If node is expanded initially, render its children
  if (isExpanded && node.children && node.children.length > 0) {
    for (const child of node.children) {
      renderTreeNode(child, depth + 1, tbody, rootTotalSize);
    }
  }
}

async function expandNodeChildren(parentNode, depth, parentTr, tbody, rootTotalSize) {
  let children = parentNode.children;

  if (!children || children.length === 0) {
    try {
      const res = await secureFetch(`/api/tree-children?node_idx=${parentNode.node_idx}`);
      if (res.ok) {
        const data = await res.json();
        children = data.children || [];
        parentNode.children = children;
      }
    } catch (err) {
      console.error('Failed to fetch tree children:', err);
    }
  }

  if (!children || children.length === 0) return;
  let insertAfterTr = parentTr;

  for (const child of children) {
    const tr = document.createElement('tr');
    tr.dataset.nodeIdx = child.node_idx;
    tr.dataset.parentIdx = parentNode.node_idx;
    tr.dataset.fullPath = child.full_path;
    tr.dataset.size = child.size;
    tr.dataset.isDir = child.is_dir;

    const pctOfParent = (rootTotalSize > 0) ? ((child.size / rootTotalSize) * 100).toFixed(1) : 0;
    const iconSvg = child.is_dir ? SVG_FOLDER : SVG_FILE;
    const hasChildren = child.has_children || (child.children && child.children.length > 0) || (child.is_dir && child.file_count + child.dir_count > 0);
    const isExpanded = expandedNodes.has(child.node_idx);
    const expanderChar = hasChildren ? (isExpanded ? '▼' : '▶') : '';

    tr.innerHTML = `
      <td>
        <div class="tree-node-wrap">
          <span class="tree-indent" style="width: ${depth * 14}px"></span>
          <span class="tree-expander">${expanderChar}</span>
          ${iconSvg}
          <span class="tree-name" title="${escapeHtml(child.full_path)}">${escapeHtml(child.name)}</span>
        </div>
      </td>
      <td>
        <div class="pct-cell-wrap">
          <div class="pct-track">
            <div class="pct-fill" style="width: ${pctOfParent}%"></div>
          </div>
          <span class="pct-val">${pctOfParent}%</span>
        </div>
      </td>
      <td class="col-size">${treemapRenderer.formatSize(child.size)}</td>
      <td class="col-alloc">${treemapRenderer.formatSize(child.allocated_size || child.size)}</td>
      <td class="col-num">${formatNumber(child.file_count || 0)}</td>
      <td class="col-num">${formatNumber(child.dir_count || 0)}</td>
    `;

    tr.addEventListener('click', () => {
      document.querySelectorAll('#tree-table-body tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      selectedNodeIdx = child.node_idx;
      document.getElementById('status-selected').textContent = `${child.full_path} (${treemapRenderer.formatSize(child.size)})`;

      const matchedTile = treemapRenderer.tiles.find(t => t.node_idx === child.node_idx);
      if (matchedTile) {
        treemapRenderer.selectedTile = matchedTile;
        treemapRenderer.render();
      }
    });

    tr.addEventListener('dblclick', () => {
      if (child.is_dir) {
        secureFetch(`/api/treemap?node_idx=${child.node_idx}`)
          .then(res => res.json())
          .then(data => {
            treemapRenderer.setTiles(data.tiles || []);
            breadcrumbPath.push({ name: child.name, node_idx: child.node_idx });
            updateBreadcrumbs();
          });
      }
    });

    const exp = tr.querySelector('.tree-expander');
    if (hasChildren && exp) {
      exp.addEventListener('click', async (e) => {
        e.stopPropagation();
        if (expandedNodes.has(child.node_idx)) {
          expandedNodes.delete(child.node_idx);
          collapseNodeRows(child.node_idx, tbody);
          exp.textContent = '▶';
        } else {
          expandedNodes.add(child.node_idx);
          exp.textContent = '▼';
          await expandNodeChildren(child, depth + 1, tr, tbody, rootTotalSize);
        }
      });
    }

    insertAfterTr.after(tr);
    insertAfterTr = tr;
  }
}

function collapseNodeRows(parentNodeIdx, tbody) {
  const rows = Array.from(tbody.querySelectorAll('tr'));
  const idx = rows.findIndex(r => Number(r.dataset.nodeIdx) === parentNodeIdx);
  if (idx < 0) return;

  const parentDepth = parseInt(rows[idx].querySelector('.tree-indent')?.style.width || '0', 10);

  for (let i = idx + 1; i < rows.length; i++) {
    const row = rows[i];
    const rowDepth = parseInt(row.querySelector('.tree-indent')?.style.width || '0', 10);
    if (rowDepth > parentDepth) {
      row.remove();
    } else {
      break;
    }
  }
}

// 8. Top 100 Largest Files
async function loadTopHogs() {
  try {
    const res = await secureFetch('/api/top-hogs?limit=100');
    if (!res.ok) return;
    const data = await res.json();
    topHogsData = data.hogs || [];
    renderTopHogsTable(topHogsData);
  } catch (err) {
    console.error('Failed to load top hogs:', err);
  }
}

function renderTopHogsTable(items) {
  const tbody = document.getElementById('hogs-table-body');
  tbody.innerHTML = '';

  if (!items || items.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" class="table-placeholder">No large files found.</td></tr>';
    return;
  }

  items.forEach((it, idx) => {
    const tr = document.createElement('tr');
    tr.dataset.fullPath = it.full_path;
    tr.dataset.name = it.name;
    tr.dataset.size = it.size;

    tr.innerHTML = `
      <td class="col-num" style="color: var(--text-dim);">${idx + 1}</td>
      <td style="font-weight: 500;" title="${escapeHtml(it.name)}">
        ${SVG_FILE}${escapeHtml(it.name)}
      </td>
      <td class="col-size" style="font-weight: 600;">${treemapRenderer.formatSize(it.size)}</td>
      <td style="font-family: var(--font-mono); color: #38bdf8;">${escapeHtml(it.extension || '[none]')}</td>
      <td style="color: var(--text-dim); font-family: var(--font-mono); font-size: 10px;" title="${escapeHtml(it.full_path)}">
        ${escapeHtml(it.full_path)}
      </td>
    `;

    tr.addEventListener('click', () => {
      document.querySelectorAll('#hogs-table-body tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      document.getElementById('status-selected').textContent = `${it.full_path} (${treemapRenderer.formatSize(it.size)})`;
    });

    tbody.appendChild(tr);
  });
}

// 9. Duplicate Candidate Finder
async function loadDuplicates() {
  try {
    const res = await secureFetch('/api/duplicates');
    if (!res.ok) return;
    const data = await res.json();
    duplicatesData = data.duplicates || [];
    renderDuplicatesTable(duplicatesData);
  } catch (err) {
    console.error('Failed to load duplicates:', err);
  }
}

function renderDuplicatesTable(items) {
  const tbody = document.getElementById('duplicates-table-body');
  tbody.innerHTML = '';

  if (!items || items.length === 0) {
    tbody.innerHTML = '<tr><td colspan="6" class="table-placeholder">No duplicate candidates found (&gt; 1 MB).</td></tr>';
    return;
  }

  items.forEach((it, idx) => {
    const tr = document.createElement('tr');
    tr.dataset.fullPath = it.paths[0] || '';
    tr.dataset.name = it.name;
    tr.dataset.size = it.size;

    tr.innerHTML = `
      <td class="col-num" style="color: var(--text-dim);">${idx + 1}</td>
      <td style="font-weight: 500;" title="${escapeHtml(it.name)}">
        ${SVG_FILE}${escapeHtml(it.name)}
      </td>
      <td class="col-size">${treemapRenderer.formatSize(it.size)}</td>
      <td class="col-size" style="font-weight: 700; color: #f59e0b;">${treemapRenderer.formatSize(it.wasted_bytes)}</td>
      <td class="col-num" style="font-weight: 600;">${it.paths.length}</td>
      <td style="color: var(--text-dim); font-family: var(--font-mono); font-size: 10px;" title="${escapeHtml(it.paths.join('\n'))}">
        ${escapeHtml(it.paths[0])} <span style="color: var(--text-muted);">(+${it.paths.length - 1} more)</span>
      </td>
    `;

    tr.addEventListener('click', () => {
      document.querySelectorAll('#duplicates-table-body tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      document.getElementById('status-selected').textContent = `${it.name} (${it.paths.length} copies, ${treemapRenderer.formatSize(it.wasted_bytes)} wasted)`;
    });

    tbody.appendChild(tr);
  });
}

function getFileTypeDescription(ext) {
  const e = (ext || '').toLowerCase();
  const map = {
    '.dll': 'Application extension',
    '[no extension]': '(No Extension)',
    '': '(No Extension)',
    '.exe': 'Application',
    '.sys': 'System file',
    '.msi': 'Windows Installer Package',
    '.bin': 'BIN File',
    '.mkv': 'MKV File',
    '.mp4': 'MP4 Video',
    '.db': 'Data Base File',
    '.lib': 'Object File Library',
    '.js': 'JSFile',
    '.cat': 'Security Catalog',
    '.pak': 'PAK File',
    '.json': 'JSON Source File',
    '.so': 'SO File',
    '.zip': 'Compressed (zipped) Folder',
    '.rar': 'WinRAR Archive',
    '.7z': '7-Zip Archive',
    '.tar': 'TAR Archive',
    '.gz': 'GZ Archive',
    '.iso': 'Disc Image File',
    '.vhd': 'Virtual Hard Disk',
    '.mp3': 'MP3 Audio',
    '.wav': 'Wave Sound',
    '.jpg': 'JPEG image',
    '.png': 'PNG image',
    '.pdf': 'PDF Document',
    '.docx': 'Word Document',
    '.txt': 'Text Document',
    '.cpp': 'C++ Source File',
    '.h': 'C/C++ Header File',
    '.py': 'Python File'
  };
  return map[e] || (e ? `${e.slice(1).toUpperCase()} File` : 'File');
}

// 10. File Extension Breakdown Table (WizTree Matrix Layout)
function renderExtensionsTable(extensions, totalBytes) {
  const tbody = document.getElementById('ext-table-body');
  tbody.innerHTML = '';

  if (!extensions || extensions.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" class="table-placeholder">No extension data available.</td></tr>';
    return;
  }

  extensions.forEach(ext => {
    const tr = document.createElement('tr');
    const pct = (totalBytes > 0) ? ((ext.total_size / totalBytes) * 100).toFixed(1) : 0;
    const typeDesc = getFileTypeDescription(ext.ext);

    tr.innerHTML = `
      <td>
        <span class="ext-swatch" style="background-color: ${ext.color};"></span>
        <b style="font-family: var(--font-mono);">${escapeHtml(ext.ext || '[no extension]')}</b>
      </td>
      <td style="color: var(--text-muted); white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${escapeHtml(typeDesc)}">
        ${escapeHtml(typeDesc)}
      </td>
      <td>
        <div class="pct-cell-wrap">
          <div class="pct-track">
            <div class="pct-fill" style="width: ${pct}%; background-color: ${ext.color};"></div>
          </div>
          <span class="pct-val">${pct}%</span>
        </div>
      </td>
      <td class="col-size">${treemapRenderer.formatSize(ext.total_size)}</td>
      <td class="col-num">${formatNumber(ext.count)}</td>
    `;

    // Click extension row to highlight in Treemap
    tr.addEventListener('click', () => {
      document.querySelectorAll('#ext-table-body tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      treemapRenderer.setHighlightedExtension(ext.ext);
      document.getElementById('status-selected').textContent = `Filtered extension: ${ext.ext || '[none]'} (${treemapRenderer.formatSize(ext.total_size)})`;
    });

    tbody.appendChild(tr);
  });
}

// 11. Treemap Tile Interactions
function handleTileClick(tile, isDblClick) {
  if (!tile) return;

  const row = document.querySelector(`tr[data-node-idx="${tile.node_idx}"]`);
  if (row) {
    document.querySelectorAll('#tree-table-body tr').forEach(r => r.classList.remove('selected'));
    row.classList.add('selected');
    row.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  document.getElementById('status-selected').textContent = `${tile.full_path} (${treemapRenderer.formatSize(tile.size)})`;

  if (isDblClick && tile.is_dir) {
    secureFetch(`/api/treemap?node_idx=${tile.node_idx}`)
      .then(res => res.json())
      .then(data => {
        treemapRenderer.setTiles(data.tiles || []);
        breadcrumbPath.push({ name: tile.name, node_idx: tile.node_idx });
        updateBreadcrumbs();
      });
  }
}

function handleTileHover(tile, clientX, clientY) {
  const tooltip = document.getElementById('treemap-tooltip');
  if (!tile) {
    tooltip.style.display = 'none';
    return;
  }

  tooltip.style.display = 'block';
  tooltip.style.left = `${Math.min(window.innerWidth - 370, clientX + 16)}px`;
  tooltip.style.top = `${Math.min(window.innerHeight - 130, clientY + 16)}px`;

  tooltip.innerHTML = `
    <div class="tt-name">${tile.name}</div>
    <div class="tt-row">
      <span>Size: <b>${treemapRenderer.formatSize(tile.size)}</b></span>
      <span>•</span>
      <span>${tile.is_dir ? 'Directory' : (tile.extension || 'File')}</span>
    </div>
    <div class="tt-path">${escapeHtml(tile.full_path)}</div>
  `;
}

function updateBreadcrumbs(initial) {
  const container = document.getElementById('breadcrumb-trail');
  container.innerHTML = '';

  if (initial) {
    breadcrumbPath = [{ name: initial[0], node_idx: currentScanData.tree_root ? currentScanData.tree_root.node_idx : 0 }];
  }

  breadcrumbPath.forEach((item, idx) => {
    if (idx > 0) {
      const sep = document.createElement('span');
      sep.className = 'breadcrumb-sep';
      sep.textContent = ' › ';
      container.appendChild(sep);
    }
    const span = document.createElement('span');
    span.className = 'breadcrumb-item';
    span.textContent = item.name;
    span.addEventListener('click', () => {
      breadcrumbPath = breadcrumbPath.slice(0, idx + 1);
      secureFetch(`/api/treemap?node_idx=${item.node_idx}`)
        .then(res => res.json())
        .then(data => {
          treemapRenderer.setTiles(data.tiles || []);
          updateBreadcrumbs();
        });
    });
    container.appendChild(span);
  });
}

function showContextMenu(x, y) {
  const menu = document.getElementById('context-menu');
  menu.style.display = 'block';
  menu.style.left = `${Math.min(window.innerWidth - 210, x)}px`;
  menu.style.top = `${Math.min(window.innerHeight - 210, y)}px`;
}

function filterActiveTable(query) {
  const q = (query || '').toLowerCase().trim();

  document.querySelectorAll('#tree-table-body tr').forEach(r => {
    if (!q) { r.style.display = ''; return; }
    r.style.display = r.textContent.toLowerCase().includes(q) ? '' : 'none';
  });

  document.querySelectorAll('#hogs-table-body tr').forEach(r => {
    if (!q) { r.style.display = ''; return; }
    r.style.display = r.textContent.toLowerCase().includes(q) ? '' : 'none';
  });

  document.querySelectorAll('#duplicates-table-body tr').forEach(r => {
    if (!q) { r.style.display = ''; return; }
    r.style.display = r.textContent.toLowerCase().includes(q) ? '' : 'none';
  });
}

function logMcpAction(msg) {
  const log = document.getElementById('mcp-log');
  if (!log) return;
  const line = document.createElement('div');
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  log.prepend(line);
}

function startMcpEventStream() {
  setInterval(async () => {
    if (document.hidden) return;
    try {
      const res = await secureFetch('/api/mcp-stats');
      if (!res.ok) return;
      const data = await res.json();

      document.getElementById('mcp-stat-queries').textContent = formatNumber(data.total_queries || 0);
      document.getElementById('mcp-stat-tokens-used').textContent = formatNumber(data.total_tokens_used || 0);
      document.getElementById('mcp-stat-tokens-saved').textContent = formatNumber(data.total_tokens_saved || 0);

      const total = (data.total_tokens_used || 0) + (data.total_tokens_saved || 0);
      const savingsPct = (total > 0) ? Math.round((data.total_tokens_saved / total) * 100) : 94;
      document.getElementById('mcp-stat-savings-pct').textContent = `${savingsPct}%`;
    } catch (e) {}
  }, 2500);
}

function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}
