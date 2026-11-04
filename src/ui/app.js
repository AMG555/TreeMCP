/**
 * TreeMCP Main Application UI Controller
 * High-speed native desktop interactivity, synchronized treemap, top hogs table.
 */

let treemapRenderer = null;
let currentScanData = null;
let currentDrives = [];
let selectedNodeIdx = null;
let breadcrumbPath = [];
let expandedNodes = new Set();
let contextTarget = null;
let topHogsData = [];

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

// Number formatting: Standard US thousands grouping (e.g. 1,249,600)
function formatNumber(num) {
  if (num === undefined || num === null) return '0';
  return Number(num).toLocaleString('en-US');
}

// Initialize when DOM ready
document.addEventListener('DOMContentLoaded', () => {
  const canvas = document.getElementById('treemap-canvas');
  treemapRenderer = new TreemapRenderer(canvas, handleTileClick, handleTileHover);
  
  // Initial size check
  setTimeout(() => treemapRenderer.resize(), 50);

  setupTabs();
  setupSplitter();
  setupEventListeners();
  loadDrives();
  startMcpEventStream();
});

// Segmented tab switching
function setupTabs() {
  const tabs = document.querySelectorAll('.tab-btn');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content-pane').forEach(p => p.classList.remove('active'));

      tab.classList.add('active');
      const targetId = tab.dataset.target;
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.classList.add('active');

      if (targetId === 'pane-top-hogs' && topHogsData.length === 0) {
        loadTopHogs();
      }
    });
  });
}

// Draggable Horizontal Splitter
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
    const newHeight = Math.max(120, Math.min(viewportHeight - 120, startHeight + dy));

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
}

function setupEventListeners() {
  const triggerScan = () => {
    const customPath = document.getElementById('custom-path').value.trim();
    const driveSelect = document.getElementById('drive-select').value;
    const pathToScan = customPath || driveSelect;
    startScan(pathToScan);
  };

  document.getElementById('btn-scan').addEventListener('click', triggerScan);

  const btnRefresh = document.getElementById('btn-refresh');
  if (btnRefresh) {
    btnRefresh.addEventListener('click', triggerScan);
  }

  document.getElementById('custom-path').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') triggerScan();
  });

  document.getElementById('drive-select').addEventListener('change', (e) => {
    document.getElementById('custom-path').value = e.target.value;
    const drv = currentDrives.find(d => d.path === e.target.value);
    if (drv) updateDriveUsage(drv);
  });

  document.getElementById('search-filter').addEventListener('input', (e) => {
    filterActiveTable(e.target.value);
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
      document.querySelector('[data-target="pane-mcp"]').click();
      logMcpAction(`Inspected item: ${contextTarget.full_path} (${treemapRenderer.formatSize(contextTarget.size)})`);
    }
  });

  const ctxRecycle = document.getElementById('ctx-safe-recycle');
  if (ctxRecycle) {
    ctxRecycle.addEventListener('click', async () => {
      if (!contextTarget || !contextTarget.full_path) return;
      const targetPath = contextTarget.full_path;
      if (confirm(`Move "${targetPath}" safely to the Recycle Bin / Trash?\n\n(Protected system paths are automatically blocked).`)) {
        try {
          const res = await secureFetch('/api/clean?path=' + encodeURIComponent(targetPath) + '&dry_run=false');
          const data = await res.json();
          if (data.success) {
            alert(`Item successfully relocated to Recycle Bin / Trash.`);
            triggerScan();
          } else {
            alert(`Operation blocked: ${data.message || 'Protected system file or access denied.'}`);
          }
        } catch (err) {
          alert(`Error communicating with backend: ${err.message}`);
        }
      }
    });
  }

  const ctxProps = document.getElementById('ctx-properties');
  if (ctxProps) {
    ctxProps.addEventListener('click', () => {
      if (!contextTarget) return;
      alert(`Properties for: ${contextTarget.name}\n\nFull Path: ${contextTarget.full_path}\nSize: ${treemapRenderer.formatSize(contextTarget.size)}\nType: ${contextTarget.is_dir ? 'Directory' : 'File'}`);
    });
  }
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
      opt.textContent = `${d.path} [${d.volume_name || 'Local Disk'}] - ${freeGb} GB free of ${totalGb} GB (${d.file_system})`;
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
  const badge = document.getElementById('drive-usage-badge');
  badge.textContent = `${drive.path} ${pct}% full (${treemapRenderer.formatSize(used)} used / ${treemapRenderer.formatSize(drive.total_bytes)} total)`;
}

async function startScan(path) {
  const btnScan = document.getElementById('btn-scan');
  btnScan.disabled = true;
  btnScan.innerHTML = '<span class="btn-icon">⏳</span><span>Scanning...</span>';

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
    logMcpAction(`Scan completed: ${path} (${formatNumber(data.stats.total_files)} files in ${(data.stats.duration_ms / 1000).toFixed(2)}s)`);

    // Pre-fetch Top 100 files in background
    loadTopHogs();
  } catch (err) {
    console.error('Scan failed:', err);
    document.getElementById('status-text').textContent = 'Scan error.';
  } finally {
    btnScan.disabled = false;
    btnScan.innerHTML = '<span class="btn-icon">▶</span><span>Scan</span>';
  }
}

function renderScanResults(data) {
  if (!data) return;

  // Header quick stats (Standard US thousands grouping)
  document.getElementById('stat-files').textContent = formatNumber(data.stats.total_files || 0);
  document.getElementById('stat-folders').textContent = formatNumber(data.stats.total_dirs || 0);
  document.getElementById('stat-size').textContent = treemapRenderer.formatSize(data.stats.total_bytes || 0);
  document.getElementById('stat-time').textContent = `${(data.stats.duration_ms / 1000).toFixed(2)}s`;
  document.getElementById('status-text').textContent = `Scan completed in ${(data.stats.duration_ms / 1000).toFixed(2)}s`;

  // Render Tree View
  renderTreeView(data.tree_root);

  // Render Extensions Table
  renderExtensionsTable(data.extensions || [], data.stats.total_bytes || 0);

  // Render Treemap Canvas (dynamically scaled to 100% canvas resolution)
  treemapRenderer.setTiles(data.treemap_tiles || []);
  updateBreadcrumbs([data.tree_root ? data.tree_root.name : 'Root']);
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
  const icon = node.is_dir ? '📁' : '📄';
  const hasChildren = node.children && node.children.length > 0;
  const isExpanded = expandedNodes.has(node.node_idx);
  const expanderChar = hasChildren ? (isExpanded ? '▼' : '▶') : '';

  tr.innerHTML = `
    <td>
      <div class="tree-node-wrap">
        <span class="tree-indent" style="width: ${depth * 16}px"></span>
        <span class="tree-expander">${expanderChar}</span>
        <span class="tree-icon">${icon}</span>
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

  // Toggle expansion
  const expander = tr.querySelector('.tree-expander');
  if (hasChildren && expander) {
    expander.addEventListener('click', (e) => {
      e.stopPropagation();
      if (expandedNodes.has(node.node_idx)) {
        expandedNodes.delete(node.node_idx);
      } else {
        expandedNodes.add(node.node_idx);
      }
      refreshVisibleTree(tbody, currentScanData.tree_root);
    });
  }

  tbody.appendChild(tr);

  if (isExpanded && hasChildren) {
    node.children.forEach(child => {
      renderTreeNode(child, depth + 1, tbody, rootTotalSize);
    });
  }
}

function refreshVisibleTree(tbody, root) {
  tbody.innerHTML = '';
  if (root) {
    renderTreeNode(root, 0, tbody, root.size);
  }
}

// Load Top 100 Largest Files
async function loadTopHogs() {
  try {
    const res = await secureFetch('/api/top-hogs?limit=100&filter=files');
    if (!res.ok) return;
    const data = await res.json();
    topHogsData = data.items || [];
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
    tr.dataset.nodeIdx = it.node_idx;
    tr.dataset.fullPath = it.path;
    tr.dataset.name = it.name;
    tr.dataset.size = it.size;

    tr.innerHTML = `
      <td class="col-num" style="color: var(--text-dim);">${idx + 1}</td>
      <td style="font-weight: 500;" title="${escapeHtml(it.name)}">
        <span class="tree-icon">📄</span>${escapeHtml(it.name)}
      </td>
      <td class="col-size" style="font-weight: 600; color: #38bdf8;">${treemapRenderer.formatSize(it.size)}</td>
      <td style="color: var(--text-muted); font-family: var(--font-mono);">${escapeHtml(it.extension || '-')}</td>
      <td style="color: var(--text-dim); font-family: var(--font-mono);" title="${escapeHtml(it.path)}">${escapeHtml(it.path)}</td>
    `;

    tr.addEventListener('click', () => {
      document.querySelectorAll('#hogs-table-body tr').forEach(r => r.classList.remove('selected'));
      tr.classList.add('selected');
      document.getElementById('status-selected').textContent = `${it.path} (${treemapRenderer.formatSize(it.size)})`;

      const matchedTile = treemapRenderer.tiles.find(t => t.node_idx === it.node_idx);
      if (matchedTile) {
        treemapRenderer.selectedTile = matchedTile;
        treemapRenderer.render();
      }
    });

    tbody.appendChild(tr);
  });
}

function renderExtensionsTable(extensions, totalBytes) {
  const tbody = document.getElementById('ext-table-body');
  tbody.innerHTML = '';

  extensions.forEach(ext => {
    const tr = document.createElement('tr');
    const pct = (totalBytes > 0) ? ((ext.total_size / totalBytes) * 100).toFixed(1) : 0;
    tr.innerHTML = `
      <td>
        <span class="ext-swatch" style="background-color: ${ext.color};"></span>
        <b style="font-family: var(--font-mono);">${escapeHtml(ext.ext || '[no extension]')}</b>
      </td>
      <td class="col-size">${treemapRenderer.formatSize(ext.total_size)}</td>
      <td>
        <div class="pct-cell-wrap">
          <div class="pct-track">
            <div class="pct-fill" style="width: ${pct}%; background-color: ${ext.color};"></div>
          </div>
          <span class="pct-val">${pct}%</span>
        </div>
      </td>
      <td class="col-num">${formatNumber(ext.count)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function handleTileClick(tile, isDblClick) {
  if (!tile) return;

  // Highlight in Tree View
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
  tooltip.style.left = `${Math.min(window.innerWidth - 360, clientX + 16)}px`;
  tooltip.style.top = `${Math.min(window.innerHeight - 120, clientY + 16)}px`;

  tooltip.innerHTML = `
    <div class="tt-name">${tile.is_dir ? '📁' : '📄'} ${escapeHtml(tile.name)}</div>
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
  menu.style.left = `${Math.min(window.innerWidth - 200, x)}px`;
  menu.style.top = `${Math.min(window.innerHeight - 200, y)}px`;
}

function filterActiveTable(query) {
  const q = (query || '').toLowerCase();
  
  // Filter Tree View
  document.querySelectorAll('#tree-table-body tr').forEach(r => {
    if (!q) { r.style.display = ''; return; }
    const text = r.textContent.toLowerCase();
    r.style.display = text.includes(q) ? '' : 'none';
  });

  // Filter Top Hogs
  document.querySelectorAll('#hogs-table-body tr').forEach(r => {
    if (!q) { r.style.display = ''; return; }
    const text = r.textContent.toLowerCase();
    r.style.display = text.includes(q) ? '' : 'none';
  });
}

function logMcpAction(msg) {
  const log = document.getElementById('mcp-log');
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
    } catch (e) {
      // ignore
    }
  }, 2000);
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
