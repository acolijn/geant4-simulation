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
  toggleVis('pos-unit-group',   needsPos);
  toggleVis('pos-type-row',     hasShape);
  toggleVis('pos-shape-group',  hasShape);

  const shCyl    = hasShape && (posShape === 'Cylinder');
  const shSphere = hasShape && (posShape === 'Sphere' || posShape === 'Circle' || posShape === 'Ellipsoid');
  const shBox    = hasShape && (posShape === 'Box');

  toggleVis('pos-radius-group',  shCyl || shSphere);
  toggleVis('pos-halfz-group',   shCyl);
  toggleVis('pos-box-group',     shBox);
  toggleVis('pos-confine-group', posType === 'Volume');

  // Angular — only show extra fields when restricting angles or focusing
  const angHasExtra = (angType === 'cos' || angType === 'focused' || angType === 'beam1d' || angType === 'beam2d');
  toggleVis('ang-theta-group', angType === 'cos');
  toggleVis('ang-focus-group', angType === 'focused');
  toggleVis('ang-dir-group',   angType === 'beam1d' || angType === 'beam2d');

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
$('geometry-select').addEventListener('change', loadVolumes);

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

$('btn-start').addEventListener('click', async () => {
  const body = {
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
    ionCharge:   $('ion-charge') ? $('ion-charge').value : '0',
    ionExcitation: $('ion-excitation') ? $('ion-excitation').value : '0',
    summarizeHits: $('summarizeHits').checked ? '1' : '0',
  };

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
});

$('btn-stop').addEventListener('click', async () => {
  await fetch('/api/stop', { method: 'POST' });
  if (ws) { ws.close(); ws = null; }
  $('btn-start').disabled = false;
  $('btn-stop').disabled = true;
  $('progress-text').textContent = 'Stopped';
  loadRunHistory();
});

// Run history
async function loadRunHistory() {
  const runs = await fetch('/api/runs').then(json);
  const tbody = $('run-history').querySelector('tbody');
  tbody.innerHTML = runs.map(r => `
    <tr>
      <td>${esc(r.runId || r.started)}</td>
      <td>${esc(r.geometry)}</td>
      <td>${esc(r.particle)} ${esc(r.energy)}${r.sourceType ? ' (' + esc(r.sourceType) + ')' : ''}</td>
      <td>${(r.nEvents || 0).toLocaleString()}</td>
      <td>${esc(r.status)}</td>
    </tr>
  `).join('');
}
loadRunHistory();

// ─── RESULTS TAB ────────────────────────────────────────

let _runsCache = [];   // cached run meta for the results tab

async function loadRunsForResults() {
  _runsCache = await fetch('/api/runs').then(json);
  const sel = $('result-run-select');
  sel.innerHTML = '<option value="">— select a run —</option>' +
    _runsCache.map(r => {
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
  if (meta.outputFile) items.push({ label: 'Output', value: meta.outputFile });

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

  // Load files
  const files = await fetch(`/api/results/${runId}/files`).then(json);
  $('result-files').innerHTML = files.map(f =>
    `<li><a href="/api/results/${encodeURIComponent(runId)}/download/${encodeURIComponent(f)}" download>${esc(f)}</a></li>`
  ).join('');

  // Load branches
  try {
    const br = await fetch(`/api/results/${runId}/branches`).then(json);
    const sel = $('branch-select');
    sel.innerHTML = (br.branches || []).map(b => `<option value="${b}">${b}</option>`).join('');
  } catch (e) {
    $('branch-select').innerHTML = '<option>no ROOT file</option>';
  }

  // Load log
  try {
    const logData = await fetch(`/api/results/${runId}/log`).then(json);
    $('result-log').textContent = logData.log || '(no log)';
  } catch (e) {
    $('result-log').textContent = '(no log available)';
  }

  $('plot-container').innerHTML = '';
});

$('btn-plot').addEventListener('click', async () => {
  const runId = $('result-run-select').value;
  const branch = $('branch-select').value;
  if (!runId || !branch) return;

  $('plot-container').innerHTML = '<p>Loading…</p>';
  const res = await fetch(`/api/results/${runId}/plot/${branch}`).then(json);
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
  const res = await fetch(`/api/results/${runId}/plot3d`).then(json);
  if (res.error) {
    $('plot-container').innerHTML = `<p>Error: ${esc(res.error)}</p>`;
    return;
  }
  const fig = JSON.parse(res.plotJSON);
  $('plot-container').innerHTML = '';
  Plotly.newPlot('plot-container', fig.data, fig.layout, { responsive: true });
});
