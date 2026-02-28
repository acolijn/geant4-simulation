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

// ─── CONFIG TAB ─────────────────────────────────────────

// Load geometry list
async function loadGeometries() {
  const files = await fetch('/api/geometries').then(json);
  const sel = $('geometry-select');
  sel.innerHTML = files.map(f => `<option value="${f}">${f}</option>`).join('');
}
loadGeometries();

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
    geometry:   $('geometry-select').value,
    particle:   $('particle').value,
    energy:     $('energy').value,
    energyUnit: $('energy-unit').value,
    posX:       $('posX').value,
    posY:       $('posY').value,
    posZ:       $('posZ').value,
    posUnit:    $('pos-unit').value,
    dirX:       $('dirX').value,
    dirY:       $('dirY').value,
    dirZ:       $('dirZ').value,
    nEvents:    $('nEvents').value,
    outputFile: $('outputFile').value,
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
      <td>${r.runId || r.started}</td>
      <td>${r.geometry}</td>
      <td>${r.particle} ${r.energy}</td>
      <td>${(r.nEvents || 0).toLocaleString()}</td>
      <td>${r.status}</td>
    </tr>
  `).join('');
}
loadRunHistory();

// ─── RESULTS TAB ────────────────────────────────────────

async function loadRunsForResults() {
  const runs = await fetch('/api/runs').then(json);
  const sel = $('result-run-select');
  sel.innerHTML = '<option value="">— select a run —</option>' +
    runs.map(r => {
      const id = r.runId || r.started;
      return `<option value="${id}">${id}  (${r.geometry}, ${r.particle}, ${(r.nEvents||0).toLocaleString()} evts, ${r.status})</option>`;
    }).join('');
}
loadRunsForResults();

$('result-run-select').addEventListener('change', async () => {
  const runId = $('result-run-select').value;
  if (!runId) return;

  // Load files
  const files = await fetch(`/api/results/${runId}/files`).then(json);
  $('result-files').innerHTML = files.map(f =>
    `<li><a href="/api/results/${runId}/download/${f}" download>${f}</a></li>`
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
    $('plot-container').innerHTML = `<p>Error: ${res.error}</p>`;
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
    $('plot-container').innerHTML = `<p>Error: ${res.error}</p>`;
    return;
  }
  const fig = JSON.parse(res.plotJSON);
  $('plot-container').innerHTML = '';
  Plotly.newPlot('plot-container', fig.data, fig.layout, { responsive: true });
});
