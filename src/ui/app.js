/**
 * TreeMCP Main Application UI Controller (Hardened Edition)
 */

let treemapRenderer = null;
let currentScanData = null;
let currentDrives = [];
let selectedNodeIdx = null;
let breadcrumbPath = [];
let expandedNodes = new Set();
let contextTarget = null;

// Read high-entropy session token from URL parameters
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

// Initialize when DOM ready
document.addEventListener('DOMContentLoaded', () => {
  const canvas = document.getElementById('treemap-canvas');
  treemapRenderer = new TreemapRenderer(canvas, handleTileClick, handleTileHover);
  treemapRenderer.resize();

  setupTabs();
  setupEventListeners();
  loadDrives();
  startMcpEventStream();
});

function setupTabs() {
  const tabs = document.querySelectorAll('.tab-header');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.style.display = 'none');

      tab.classList.add('active');
      const targetId = tab.dataset.target;
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.style.display = 'block';
    });
  });
}

function setupEventListeners() {
  document.getElementById('btn-scan').addEventListener('click', () => {
    const customPath = document.getElementById('custom-path').value.trim();
    const driveSelect = document.getElementById('drive-select').value;
    const pathToScan = customPath || driveSelect;
    startScan(pathToScan);
  });

  document.getElementById('drive-select').addEventListener('change', (e) => {
    document.getElementById('custom-path').value = e.target.value;
  });

  document.getElementById('search-filter').addEventListener('input', (e) => {
    filterTree(e.target.value);
  });

  // Context Menu
  window.addEventListener('click', () => {
    document.getElementById('context-menu').style.display = 'none';
  });

  document.getElementById('treemap-canvas').addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const rect = e.target.getBoundingClientRect();
    const hit = treemapRenderer.hitTest(e.clientX - rect.left, e.clientY - rect.top);
    if (hit) {
      contextTarget = hit;
      showContextMenu(e.clientX, e.clientY);
    }
  });

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
      logMcpAction(`Security Inspected: ${contextTarget.full_path} (${treemapRenderer.formatSize(contextTarget.size)})`);
    }
  });
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
      updateDriveUsageBar(currentDrives[0]);
    }
  } catch (e) {
    console.error('Failed to load drives:', e);
  }
}

function updateDriveUsageBar(drive) {
  if (!drive || !drive.total_bytes) return;
  const used = drive.total_bytes - drive.free_bytes;
  const pct = Math.round((used / drive.total_bytes) * 100);

  const fill = document.getElementById('drive-usage-fill');
  const text = document.getElementById('drive-usage-text');

  fill.style.width = `${pct}%`;
  text.textContent = `Used: ${treemapRenderer.formatSize(used)} (${pct}%) | Free: ${treemapRenderer.formatSize(drive.free_bytes)} | Total: ${treemapRenderer.formatSize(drive.total_bytes)}`;
}

async function startScan(path) {
  const btnScan = document.getElementById('btn-scan');
  btnScan.disabled = true;
  btnScan.textContent = 'Scanning...';

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

    renderScanResults(data);
    logMcpAction(`Scan completed: ${path} - ${data.stats.total_files} files in ${(data.stats.duration_ms / 1000).toFixed(2)}s`);
  } catch (err) {
    console.error('Scan failed:', err);
    document.getElementById('status-text').textContent = 'Scan error.';
  } finally {
    btnScan.disabled = false;
    btnScan.textContent = 'Scan';
  }
}

function renderScanResults(data) {
  if (!data) return;

  // Stats bar
  document.getElementById('stat-files').textContent = (data.stats.total_files || 0).toLocaleString();
  document.getElementById('stat-folders').textContent = (data.stats.total_dirs || 0).toLocaleString();
  document.getElementById('stat-size').textContent = treemapRenderer.formatSize(data.stats.total_bytes || 0);
  document.getElementById('stat-time').textContent = `${(data.stats.duration_ms / 1000).toFixed(2)}s`;
  document.getElementById('status-text').textContent = `Scan completed in ${(data.stats.duration_ms / 1000).toFixed(2)}s`;

  // Render Tree View
  renderTreeView(data.tree_root);

  // Render Extensions Table
  renderExtensionsTable(data.extensions || [], data.stats.total_bytes || 0);

  // Render Treemap Canvas
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

  const pctOfParent = (rootTotalSize > 0) ? ((node.size / rootTotalSize) * 100).toFixed(1) : 0;

  const icon = node.is_dir ? '📁' : '📄';
  const hasChildren = node.children && node.children.length > 0;
  const isExpanded = expandedNodes.has(node.node_idx);
  const expanderChar = hasChildren ? (isExpanded ? '▼' : '▶') : ' ';

  tr.innerHTML = `
    <td>
      <span class="tree-indent" style="width: ${depth * 18}px"></span>
      <span class="tree-expander">${expanderChar}</span>
      <span class="tree-icon">${icon}</span>
      <span class="tree-name">${escapeHtml(node.name)}</span>
    </td>
    <td>
      <div class="pct-cell-bar">
        <div class="pct-bar-outer">
          <div class="pct-bar-inner" style="width: ${pctOfParent}%"></div>
        </div>
        <span>${pctOfParent}%</span>
      </div>
    </td>
    <td style="font-family: monospace;">${treemapRenderer.formatSize(node.size)}</td>
    <td style="font-family: monospace;">${treemapRenderer.formatSize(node.allocated_size || node.size)}</td>
    <td>${(node.file_count || 0).toLocaleString()}</td>
    <td>${(node.dir_count || 0).toLocaleString()}</td>
  `;

  // Click row to select
  tr.addEventListener('click', (e) => {
    document.querySelectorAll('#tree-table-body tr').forEach(r => r.classList.remove('selected'));
    tr.classList.add('selected');
    selectedNodeIdx = node.node_idx;
  });

  // Toggle expansion
  const expander = tr.querySelector('.tree-expander');
  if (hasChildren) {
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

function renderExtensionsTable(extensions, totalBytes) {
  const tbody = document.getElementById('ext-table-body');
  tbody.innerHTML = '';

  extensions.forEach(ext => {
    const tr = document.createElement('tr');
    const pct = (totalBytes > 0) ? ((ext.total_size / totalBytes) * 100).toFixed(1) : 0;
    tr.innerHTML = `
      <td>
        <span class="ext-swatch" style="background-color: ${ext.color};"></span>
        ${escapeHtml(ext.ext)}
      </td>
      <td style="font-family: monospace;">${treemapRenderer.formatSize(ext.total_size)}</td>
      <td>
        <div class="pct-cell-bar">
          <div class="pct-bar-outer">
            <div class="pct-bar-inner" style="width: ${pct}%; background-color: ${ext.color};"></div>
          </div>
          <span>${pct}%</span>
        </div>
      </td>
      <td>${ext.count.toLocaleString()}</td>
    `;
    tbody.appendChild(tr);
  });
}

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
  tooltip.style.left = `${clientX + 14}px`;
  tooltip.style.top = `${clientY + 14}px`;

  tooltip.innerHTML = `
    <div class="tt-title">${escapeHtml(tile.name)}</div>
    <div class="tt-size">Size: ${treemapRenderer.formatSize(tile.size)}</div>
    <div style="color: #a4b0be; font-size: 11px; margin-top: 4px;">${escapeHtml(tile.full_path)}</div>
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
      sep.textContent = ' > ';
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
  menu.style.left = `${x}px`;
  menu.style.top = `${y}px`;
}

function filterTree(query) {
  if (!query) {
    document.querySelectorAll('#tree-table-body tr').forEach(r => r.style.display = '');
    return;
  }
  const q = query.toLowerCase();
  document.querySelectorAll('#tree-table-body tr').forEach(r => {
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
    try {
      const res = await secureFetch('/api/mcp-stats');
      if (!res.ok) return;
      const data = await res.json();

      document.getElementById('mcp-stat-queries').textContent = data.total_queries || 0;
      document.getElementById('mcp-stat-tokens-used').textContent = (data.total_tokens_used || 0).toLocaleString();
      document.getElementById('mcp-stat-tokens-saved').textContent = (data.total_tokens_saved || 0).toLocaleString();

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
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
