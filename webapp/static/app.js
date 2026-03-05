/* ── G4sim Dashboard – client-side logic ─────────────────── */

// ─── Tab switching ──────────────────────────────────────
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(s => s.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');

    // Refresh data when switching tabs
    if (btn.dataset.tab === 'run') loadRunHistory();
    if (btn.dataset.tab === 'results') loadRunsForResults();
  });
});

// ─── Helpers ────────────────────────────────────────────
const $ = id => document.getElementById(id);
const json = r => r.json();

/** Escape HTML special characters to prevent XSS. */
function esc(s) {
  if (s == null) return '';
  const d = document.createElement('div');
  d.appendChild(document.createTextNode(String(s)));
  return d.innerHTML;
}

// ─── GPS dynamic form visibility ────────────────────────

function toggleRunMode() {
  const mode = $('run-mode').value;
  toggleVis('condor-njobs-group', mode === 'condor');
  toggleVis('condor-merge-group', mode === 'condor');
}
toggleRunMode();

function toggleGpsFields() {
  const eneType = $('ene-type').value;
  const posType = $('pos-type').value;
  const posShape = $('pos-shape') ? $('pos-shape').value : '';
  const angType = $('ang-type').value;
  const particle = $('particle').value;

  // Ion — show ion dropdown when particle == 'ion', hide energy fields
  const isIon = (particle === 'ion');
  toggleVis('ion-select-group', isIon);
  toggleVis('ene-type-group',  !isIon);
  toggleVis('ene-value-group', !isIon);

  // Energy — only show sub-fields when relevant (and not ion)
  const isGauss = (!isIon && eneType === 'Gauss');
  const isRange = (!isIon && (eneType === 'Lin' || eneType === 'Pow'));
  const isPow   = (!isIon && eneType === 'Pow');
  const isLin   = (!isIon && eneType === 'Lin');

  toggleVis('ene-sigma-group', isGauss);
  toggleVis('ene-range-group', isRange);
  toggleVis('ene-alpha-group', isPow);
  toggleVis('ene-lin-group',   isLin);

  // Position — show centre/unit always; shape/dimensions only for Volume/Surface
  const hasShape = (posType === 'Volume' || posType === 'Surface');
  const needsPos = true;  // all source types need a position

  toggleVis('sep-position',     true);
  toggleVis('hdr-position',     true);
  toggleVis('pos-centre-group', needsPos);
  toggleVis('pos-type-row',     hasShape);
  toggleVis('pos-shape-group',  hasShape);

  const shCyl    = hasShape && (posShape === 'Cylinder');
  const shSphere = hasShape && (posShape === 'Sphere' || posShape === 'Circle' || posShape === 'Ellipsoid');
  const shBox    = hasShape && (posShape === 'Box');

  toggleVis('pos-radius-group',  shCyl || shSphere);
  toggleVis('pos-halfz-group',   shCyl);
  toggleVis('pos-box-group',     shBox);
  toggleVis('pos-confine-group', posType === 'Volume');

  // Angular — hide entirely for ions (decay is always isotropic)
  toggleVis('ang-type-group', !isIon);
  const angHasExtra = !isIon && (angType === 'cos' || angType === 'focused' || angType === 'beam1d' || angType === 'beam2d');
  toggleVis('ang-theta-group', !isIon && angType === 'cos');
  toggleVis('ang-focus-group', !isIon && angType === 'focused');
  toggleVis('ang-dir-group',   !isIon && (angType === 'beam1d' || angType === 'beam2d'));

  // Hide section headers & separators when nothing extra is shown
  toggleVis('sep-position', needsPos);
  toggleVis('hdr-position', needsPos);
  toggleVis('sep-angular',  angHasExtra);
  toggleVis('hdr-angular',  angHasExtra);
}

function toggleVis(id, show) {
  const el = $(id);
  if (!el) return;
  if (show) el.classList.remove('gps-hidden');
  else      el.classList.add('gps-hidden');
}

// Initialise GPS form on load
toggleGpsFields();

// ─── CONFIG TAB ─────────────────────────────────────────

// Load geometry list
async function loadGeometries() {
  const files = await fetch('/api/geometries').then(json);
  const sel = $('geometry-select');
  sel.innerHTML = files.map(f => `<option value="${esc(f)}">${esc(f)}</option>`).join('');
  // Load volumes for the initially selected geometry
  await loadVolumes();
  // Show geometry preview
  await loadGeometryPreview();
}

// Fetch physical volume names for the selected geometry
async function loadVolumes() {
  const geo = $('geometry-select').value;
  if (!geo) return;
  try {
    const volumes = await fetch(`/api/geometries/${encodeURIComponent(geo)}/volumes`).then(json);
    const sel = $('pos-confine');
    sel.innerHTML = '<option value="">— none (whole geometry) —</option>' +
      volumes.map(v => `<option value="${esc(v)}">${esc(v)}</option>`).join('');
  } catch (e) {
    $('pos-confine').innerHTML = '<option value="">— error loading volumes —</option>';
  }
}

loadGeometries();

// Refresh volumes when geometry changes
$('geometry-select').addEventListener('change', () => {
  loadVolumes();
  loadGeometryPreview();
});

// Mini 3-D geometry preview
async function loadGeometryPreview() {
  const geo = $('geometry-select').value;
  const container = $('geometry-preview');
  if (!geo) { container.innerHTML = ''; return; }
  try {
    const res = await fetch(`/api/geometries/${encodeURIComponent(geo)}/preview`).then(json);
    if (res.error) { container.innerHTML = ''; return; }
    const fig = JSON.parse(res.plotJSON);
    Plotly.newPlot(container, fig.data, fig.layout, {
      responsive: true,
      displayModeBar: false,
    });
  } catch (e) {
    container.innerHTML = '';
  }
}

// Upload geometry
$('geometry-upload').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const text = await file.text();
  const data = JSON.parse(text);
  const name = file.name.replace('.json', '');
  await fetch('/api/upload-geometry', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name, data }),
  });
  await loadGeometries();
  $('geometry-select').value = file.name;
});

// ─── RUN TAB ────────────────────────────────────────────

let ws = null;

/** Collect the GPS form fields into a request body. */
function collectRunBody() {
  return {
    geometry:    $('geometry-select').value,
    particle:    $('particle').value,
    eneType:     $('ene-type').value,
    energy:      $('energy').value,
    energyUnit:  $('energy-unit').value,
    eneMin:      $('ene-min').value,
    eneMax:      $('ene-max').value,
    eneSigma:    $('ene-sigma').value,
    eneAlpha:    $('ene-alpha').value,
    eneGradient: $('ene-gradient').value,
    eneIntercept:$('ene-intercept').value,
    posType:     $('pos-type').value,
    posShape:    $('pos-shape') ? $('pos-shape').value : '',
    posX:        $('posX').value,
    posY:        $('posY').value,
    posZ:        $('posZ').value,
    posUnit:     $('pos-unit').value,
    posRadius:   $('pos-radius').value,
    posHalfz:    $('pos-halfz').value,
    posHalfx:    $('pos-halfx').value,
    posHalfy:    $('pos-halfy').value,
    posHalfzBox: $('pos-halfz-box').value,
    posConfine:  $('pos-confine') ? $('pos-confine').value : '',
    angType:     $('ang-type').value,
    angMintheta: $('ang-mintheta').value,
    angMaxtheta: $('ang-maxtheta').value,
    angMinphi:   $('ang-minphi').value,
    angMaxphi:   $('ang-maxphi').value,
    angFx:       $('ang-fx').value,
    angFy:       $('ang-fy').value,
    angFz:       $('ang-fz').value,
    dirX:        $('dirX') ? $('dirX').value : '1',
    dirY:        $('dirY') ? $('dirY').value : '0',
    dirZ:        $('dirZ') ? $('dirZ').value : '0',
    nEvents:     $('nEvents').value,
    outputFile:  $('outputFile').value,
    ionZA:       $('ion-select') ? $('ion-select').value : '',
    ionName:     $('ion-select') ? $('ion-select').options[$('ion-select').selectedIndex].text : '',
    ionCharge:   $('ion-charge') ? $('ion-charge').value : '0',
    ionExcitation: $('ion-excitation') ? $('ion-excitation').value : '0',
    summarizeHits: $('summarizeHits').checked ? '1' : '0',
  };
}

$('btn-start').addEventListener('click', async () => {
  const mode = $('run-mode').value;
  if (mode === 'condor') {
    await startCondorRun();
  } else {
    await startLocalRun();
  }
});

// ─── Local run ──────────────────────────────────────────

async function startLocalRun() {
  const body = collectRunBody();

  const res = await fetch('/api/run', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }).then(json);

  if (res.error) {
    alert(res.error);
    return;
  }

  // Switch to Run tab
  document.querySelector('[data-tab="run"]').click();

  $('btn-start').disabled = true;
  $('btn-stop').disabled = false;
  $('log-output').textContent = '';
  $('progress-area').classList.remove('hidden');
  $('progress-fill').style.width = '0%';
  $('progress-text').textContent = 'Starting…';
  $('local-log-card').classList.remove('hidden');

  const nEvents = parseInt(body.nEvents);

  // Open WebSocket for live log
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  ws = new WebSocket(`${proto}://${location.host}/ws/log`);
  ws.onmessage = (e) => {
    if (e.data === '__DONE__') {
      $('btn-start').disabled = false;
      $('btn-stop').disabled = true;
      $('progress-fill').style.width = '100%';
      $('progress-text').textContent = 'Completed';
      ws.close();
      ws = null;
      loadRunHistory();
      return;
    }
    $('log-output').textContent += e.data + '\n';
    $('log-output').scrollTop = $('log-output').scrollHeight;

    // Parse progress from ">>> Event: NNN"
    const m = e.data.match(/>>> Event:\s*(\d+)/);
    if (m) {
      const evtNum = parseInt(m[1]);
      const pct = Math.min(100, (evtNum / nEvents) * 100);
      $('progress-fill').style.width = pct.toFixed(1) + '%';
      $('progress-text').textContent = `Event ${evtNum.toLocaleString()} / ${nEvents.toLocaleString()} (${pct.toFixed(0)}%)`;
    }
  };
  ws.onclose = () => {
    $('btn-start').disabled = false;
    $('btn-stop').disabled = true;
  };
}

$('btn-stop').addEventListener('click', async () => {
  await fetch('/api/stop', { method: 'POST' });
  if (ws) { ws.close(); ws = null; }
  $('btn-start').disabled = false;
  $('btn-stop').disabled = true;
  $('progress-text').textContent = 'Stopped';
  loadRunHistory();
});

// ─── Condor run ─────────────────────────────────────────

let _queuePollTimer = null;    // persistent queue polling
let _condorAvailable = false;
let _clusterCache = {};        // cid → { jobs, lastSeen, disappeared }
const CLUSTER_RETAIN_MS = 30 * 60 * 1000;  // keep completed clusters 30 min

/** Check if Condor is available and show/hide the queue panel. */
async function initCondorPanel() {
  try {
    const r = await fetch('/api/condor/available').then(json);
    _condorAvailable = r.available;
  } catch { _condorAvailable = false; }

  // Enable/disable the HTCondor option in the run-mode dropdown
  const condorOpt = document.querySelector('#run-mode option[value="condor"]');
  if (condorOpt) {
    condorOpt.disabled = !_condorAvailable;
    if (!_condorAvailable) condorOpt.textContent = 'HTCondor (not available)';
  }

  if (_condorAvailable) {
    $('condor-queue-card').classList.remove('hidden');
    startQueuePolling();
  } else {
    $('condor-queue-card').classList.add('hidden');
    // Reset to local if condor was somehow selected
    if ($('run-mode').value === 'condor') {
      $('run-mode').value = 'local';
      toggleRunMode();
    }
  }
}
initCondorPanel();

async function startCondorRun() {
  const body = collectRunBody();
  body.nCondorJobs = $('nCondorJobs').value;
  body.runMode = 'condor';
  body.autoMerge = $('autoMerge').checked;

  $('btn-start').disabled = true;
  const res = await fetch('/api/condor/submit', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }).then(json);
  $('btn-start').disabled = false;   // re-enable immediately

  if (res.error) {
    alert(res.error);
    return;
  }

  // Switch to Run tab
  document.querySelector('[data-tab="run"]').click();

  // Immediate queue refresh
  pollCondorQueue();
  loadRunHistory();
}

// ─── Persistent queue polling ───────────────────────────

function startQueuePolling() {
  stopQueuePolling();
  pollCondorQueue();   // first poll immediately
  _queuePollTimer = setInterval(pollCondorQueue, 8000);
}

function stopQueuePolling() {
  if (_queuePollTimer) { clearInterval(_queuePollTimer); _queuePollTimer = null; }
}

async function pollCondorQueue() {
  if (!_condorAvailable) return;
  try {
    const q = await fetch('/api/condor/queue').then(json);
    renderCondorQueue(q);
  } catch (e) {
    console.warn('Queue poll error:', e);
  }
}

function renderCondorQueue(q) {
  const now = Date.now();
  const liveClusters = {};

  // Group live jobs by cluster
  for (const j of (q.jobs || [])) {
    const cid = j.cluster_id;
    if (!liveClusters[cid]) liveClusters[cid] = [];
    liveClusters[cid].push(j);
  }

  // Update cache: mark live clusters, detect disappeared ones
  for (const [cid, jobs] of Object.entries(liveClusters)) {
    _clusterCache[cid] = { jobs, lastSeen: now, disappeared: null };
  }
  for (const [cid, entry] of Object.entries(_clusterCache)) {
    if (!liveClusters[cid] && !entry.disappeared) {
      // Just disappeared from queue — mark all jobs as completed
      entry.disappeared = now;
      entry.jobs = entry.jobs.map(j => ({ ...j, status: 'completed' }));
    }
    // Purge if disappeared more than 30 min ago
    if (entry.disappeared && (now - entry.disappeared) > CLUSTER_RETAIN_MS) {
      delete _clusterCache[cid];
    }
  }

  // Merge live + cached clusters for display
  const allClusters = { ...liveClusters };
  for (const [cid, entry] of Object.entries(_clusterCache)) {
    if (!allClusters[cid]) allClusters[cid] = entry.jobs;
  }

  const totalDisplay = Object.values(allClusters).reduce((s, jobs) => s + jobs.length, 0);

  // Recount across all displayed jobs
  const c = {};
  for (const jobs of Object.values(allClusters)) {
    for (const j of jobs) { c[j.status] = (c[j.status] || 0) + 1; }
  }

  // Badge in header
  $('condor-queue-badge').textContent = totalDisplay > 0 ? totalDisplay : '';

  // Summary counts
  let parts = [];
  if (c.idle)    parts.push(`<span class="cc-idle">Idle: ${c.idle}</span>`);
  if (c.running) parts.push(`<span class="cc-run">Running: ${c.running}</span>`);
  if (c.held)    parts.push(`<span class="cc-held">Held: ${c.held}</span>`);
  if (c.completed) parts.push(`<span class="cc-done">Done: ${c.completed}</span>`);
  $('condor-queue-summary').innerHTML = parts.join('');

  const container = $('condor-queue-clusters');

  if (totalDisplay === 0) {
    $('condor-queue-empty').classList.remove('hidden');
    container.innerHTML = '';
    return;
  }

  $('condor-queue-empty').classList.add('hidden');

  // Build collapsible sections per cluster
  let html = '';
  for (const [cid, jobs] of Object.entries(allClusters)) {
    const cc = {};
    for (const j of jobs) { cc[j.status] = (cc[j.status] || 0) + 1; }
    const nJobs = jobs.length;
    const isRetained = _clusterCache[cid]?.disappeared;

    let badges = [];
    if (cc.idle)    badges.push(`<span class="condor-badge condor-idle">${cc.idle} idle</span>`);
    if (cc.running) badges.push(`<span class="condor-badge condor-running">${cc.running} run</span>`);
    if (cc.completed) badges.push(`<span class="condor-badge condor-completed">${cc.completed} done</span>`);
    if (cc.held)    badges.push(`<span class="condor-badge condor-held">${cc.held} held</span>`);

    // Show time-ago for retained clusters
    let timeNote = '';
    if (isRetained) {
      const ago = Math.round((now - isRetained) / 60000);
      timeNote = `<span class="cluster-ago">${ago}m ago</span>`;
    }

    const fadeCls = isRetained ? ' cluster-faded' : '';

    html += `<details class="cluster-group${fadeCls}">
      <summary class="cluster-summary">
        <span class="cluster-id">Cluster ${cid}</span>
        <span class="cluster-count">${nJobs} jobs</span>
        ${badges.join(' ')}
        ${timeNote}
        ${isRetained ? '' : `<button class="btn-sm btn-cancel-cluster" data-cluster="${cid}" title="Cancel">✕</button>`}
      </summary>
      <table class="cluster-jobs-table">
        <thead><tr><th>Job</th><th>Status</th></tr></thead>
        <tbody>
          ${jobs.map(j => `<tr>
            <td>${cid}.${j.proc_id}</td>
            <td><span class="condor-badge condor-${j.status}">${esc(j.status)}</span></td>
          </tr>`).join('')}
        </tbody>
      </table>
    </details>`;
  }
  container.innerHTML = html;

  // Attach cancel handlers
  container.querySelectorAll('.btn-cancel-cluster').forEach(btn => {
    btn.addEventListener('click', async (e) => {
      e.preventDefault();
      e.stopPropagation();
      const cid = btn.dataset.cluster;
      if (!confirm(`Cancel all jobs in cluster ${cid}?`)) return;
      await fetch(`/api/condor/cancel-cluster/${cid}`, { method: 'POST' });
      pollCondorQueue();
      loadRunHistory();
    });
  });
}

// Run history
async function loadRunHistory() {
  const runs = await fetch('/api/runs').then(json);
  const tbody = $('run-history').querySelector('tbody');
  tbody.innerHTML = runs.map(r => {
    const sc = 'status-' + (r.status || 'unknown');
    return `
    <tr>
      <td>${esc(r.runId || r.started)}</td>
      <td>${esc(r.geometry)}</td>
      <td>${esc(r.particle)} ${esc(r.energy)}${r.sourceType ? ' (' + esc(r.sourceType) + ')' : ''}</td>
      <td>${(r.nEvents || 0).toLocaleString()}</td>
      <td>${esc(r.runMode === 'condor' ? 'Condor' : 'Local')}</td>
      <td><span class="run-status ${sc}">${esc(r.status)}</span></td>
    </tr>
  `}).join('');
}
loadRunHistory();

// ─── RESULTS TAB ────────────────────────────────────────

let _runsCache = [];   // cached run meta for the results tab

async function loadRunsForResults() {
  _runsCache = await fetch('/api/runs').then(json);
  populateFilterDropdowns();
  filterRuns();
}

/** Build unique-value lists for geometry & particle filter dropdowns. */
function populateFilterDropdowns() {
  const geoms = [...new Set(_runsCache.map(r => r.geometry).filter(Boolean))].sort();
  const parts = [...new Set(_runsCache.map(r => r.particle).filter(Boolean))].sort();
  const selGeom = $('filter-geometry');
  const selPart = $('filter-particle');
  selGeom.innerHTML = '<option value="">All</option>' + geoms.map(g => `<option value="${esc(g)}">${esc(g)}</option>`).join('');
  selPart.innerHTML = '<option value="">All</option>' + parts.map(p => `<option value="${esc(p)}">${esc(p)}</option>`).join('');
}

/** Apply filters and rebuild the run dropdown. */
function filterRuns() {
  const fGeom   = $('filter-geometry').value;
  const fPart   = $('filter-particle').value;

  const filtered = _runsCache.filter(r => {
    if (fGeom && r.geometry !== fGeom) return false;
    if (fPart && r.particle !== fPart) return false;
    return true;
  });

  const sel = $('result-run-select');
  sel.innerHTML = '<option value="">— select a run —</option>' +
    filtered.map(r => {
      const id = r.runId || r.started;
      return `<option value="${esc(id)}">${esc(id)}  (${esc(r.geometry)}, ${esc(r.particle)}, ${(r.nEvents||0).toLocaleString()} evts, ${esc(r.status)})</option>`;
    }).join('');
}
loadRunsForResults();

/** Build attractive meta-info display from a run's metadata. */
function renderRunInfo(meta) {
  const infoCard = $('run-info-card');
  if (!meta) { infoCard.classList.add('hidden'); return; }
  infoCard.classList.remove('hidden');

  // Format timing
  function fmtTs(ts) {
    if (!ts) return '—';
    // "20260301_213730" → "2026-03-01 21:37:30"
    const m = ts.match(/(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})/);
    return m ? `${m[1]}-${m[2]}-${m[3]}  ${m[4]}:${m[5]}:${m[6]}` : ts;
  }
  function duration(start, end) {
    if (!start || !end) return null;
    const p = s => { const m = s.match(/(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})/); return m ? new Date(`${m[1]}-${m[2]}-${m[3]}T${m[4]}:${m[5]}:${m[6]}`) : null; };
    const a = p(start), b = p(end);
    if (!a || !b) return null;
    const sec = Math.round((b - a) / 1000);
    if (sec < 60) return `${sec}s`;
    const min = Math.floor(sec / 60);
    return `${min}m ${sec % 60}s`;
  }

  const statusClass = 'status-' + (meta.status || 'unknown');
  const dur = duration(meta.started, meta.finished);

  const items = [
    { label: 'Status',      value: meta.status || '—',                  cls: statusClass },
    { label: 'Geometry',    value: meta.geometry || '—' },
    { label: 'Particle',    value: meta.particle || '—' },
    { label: 'Energy',      value: meta.energy || '—' },
    { label: 'Source',      value: meta.sourceType || '—' },
    { label: 'Angular',     value: meta.angularType || '—' },
    { label: 'Events',      value: (meta.nEvents || 0).toLocaleString() },
    { label: 'Summarised',  value: meta.summarizeHits === '1' ? 'Yes' : 'No' },
    { label: 'Started',     value: fmtTs(meta.started) },
    { label: 'Finished',    value: fmtTs(meta.finished) },
  ];
  if (dur) items.push({ label: 'Duration', value: dur });
  if (meta.outputFile) {
    // Build compact output description: e.g. "G4sim<000:009>.root"
    const nj = meta.n_jobs || 0;
    const base = meta.outputFile.replace('.root', '');
    if (nj > 1) {
      const last = String(nj - 1).padStart(3, '0');
      items.push({ label: 'Output', value: `${base}<000:${last}>.root` });
    } else {
      items.push({ label: 'Output', value: meta.outputFile });
    }
  }

  $('meta-grid').innerHTML = items.map(it =>
    `<div class="meta-item"><div class="meta-label">${esc(it.label)}</div><div class="meta-value ${it.cls || ''}">${esc(it.value)}</div></div>`
  ).join('');
}

$('result-run-select').addEventListener('change', async () => {
  const runId = $('result-run-select').value;
  if (!runId) { renderRunInfo(null); return; }

  // Show meta info from cache
  const meta = _runsCache.find(r => (r.runId || r.started) === runId);
  renderRunInfo(meta);

  // Load ROOT files for the file selector
  try {
    const rf = await fetch(`/api/results/${runId}/root-files`).then(json);
    const rootFiles = rf.files || [];
    const sel = $('root-file-select');
    sel.innerHTML = '<option value="">— auto (first) —</option>' +
      rootFiles.map(f => `<option value="${esc(f)}">${esc(f)}</option>`).join('');
  } catch(e) {
    $('root-file-select').innerHTML = '<option value="">— no ROOT files —</option>';
  }

  // Load all files for download section
  const files = await fetch(`/api/results/${runId}/files`).then(json);
  $('result-files').innerHTML = files.map(f =>
    `<li><a href="/api/results/${encodeURIComponent(runId)}/download/${encodeURIComponent(f)}" download>${esc(f)}</a></li>`
  ).join('');

  // Load branches for the initially selected ROOT file
  await loadBranches(runId);

  // Load log
  try {
    const logData = await fetch(`/api/results/${runId}/log`).then(json);
    $('result-log').textContent = logData.log || '(no log)';
  } catch (e) {
    $('result-log').textContent = '(no log available)';
  }

  $('plot-container').innerHTML = '';
});

// Reload branches when ROOT file selection changes
$('root-file-select').addEventListener('change', async () => {
  const runId = $('result-run-select').value;
  if (runId) await loadBranches(runId);
});

async function loadBranches(runId) {
  const rootFile = $('root-file-select').value;
  const qs = rootFile ? `?file=${encodeURIComponent(rootFile)}` : '';
  try {
    const br = await fetch(`/api/results/${runId}/branches${qs}`).then(json);
    const sel = $('branch-select');
    sel.innerHTML = (br.branches || []).map(b => `<option value="${b}">${b}</option>`).join('');
  } catch (e) {
    $('branch-select').innerHTML = '<option>no ROOT file</option>';
  }
}

function _selectedFileQS() {
  const f = $('root-file-select').value;
  return f ? `?file=${encodeURIComponent(f)}` : '';
}

$('btn-plot').addEventListener('click', async () => {
  const runId = $('result-run-select').value;
  const branch = $('branch-select').value;
  if (!runId || !branch) return;

  $('plot-container').innerHTML = '<p>Loading…</p>';
  const qs = _selectedFileQS();
  const sep = qs ? '&' : '?';
  const url = `/api/results/${runId}/plot/${branch}${qs}`;
  const res = await fetch(url).then(json);
  if (res.error) {
    $('plot-container').innerHTML = `<p>Error: ${esc(res.error)}</p>`;
    return;
  }
  const fig = JSON.parse(res.plotJSON);
  $('plot-container').innerHTML = '';
  Plotly.newPlot('plot-container', fig.data, fig.layout, { responsive: true });
});

$('btn-plot3d').addEventListener('click', async () => {
  const runId = $('result-run-select').value;
  if (!runId) return;

  $('plot-container').innerHTML = '<p>Loading 3D hit map…</p>';
  const qs = _selectedFileQS();
  const res = await fetch(`/api/results/${runId}/plot3d${qs}`).then(json);
  if (res.error) {
    $('plot-container').innerHTML = `<p>Error: ${esc(res.error)}</p>`;
    return;
  }
  const fig = JSON.parse(res.plotJSON);
  $('plot-container').innerHTML = '';
  Plotly.newPlot('plot-container', fig.data, fig.layout, { responsive: true });
});
