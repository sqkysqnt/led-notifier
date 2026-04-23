#ifndef WEB_UI_H
#define WEB_UI_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LED Notifier</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#1a1a2e;color:#e0e0e0;min-height:100vh}
.header{background:#16213e;padding:16px 20px;display:flex;align-items:center;gap:12px;border-bottom:2px solid #0f3460}
.header h1{font-size:1.3em;color:#e94560}
.header .dot{width:12px;height:12px;border-radius:50%;background:#00ff88;animation:pulse-dot 2s infinite}
@keyframes pulse-dot{0%,100%{opacity:1}50%{opacity:.4}}
.tabs{display:flex;background:#16213e;border-bottom:1px solid #0f3460}
.tab{padding:12px 20px;cursor:pointer;border:none;background:none;color:#888;font-size:.95em;transition:.2s}
.tab:hover{color:#e0e0e0}
.tab.active{color:#e94560;border-bottom:2px solid #e94560}
.content{padding:20px;max-width:800px;margin:0 auto}
.panel{display:none}
.panel.active{display:block}
.card{background:#16213e;border-radius:8px;padding:16px;margin-bottom:12px;border:1px solid #0f3460}
.card h3{color:#e94560;margin-bottom:12px;font-size:1em}
label{display:block;font-size:.85em;color:#888;margin:8px 0 4px}
input[type="text"],input[type="number"],select{width:100%;padding:8px 12px;background:#0a0a1a;border:1px solid #0f3460;border-radius:4px;color:#e0e0e0;font-size:.95em}
input[type="text"]:focus,input[type="number"]:focus,select:focus{outline:none;border-color:#e94560}
input[type="color"]{width:50px;height:34px;border:none;background:none;cursor:pointer}
input[type="range"]{width:100%;accent-color:#e94560}
.row{display:flex;gap:12px;align-items:flex-end;flex-wrap:wrap}
.row>*{flex:1;min-width:120px}
.btn{padding:8px 16px;border:none;border-radius:4px;cursor:pointer;font-size:.9em;transition:.2s}
.btn-primary{background:#e94560;color:#fff}
.btn-primary:hover{background:#c73650}
.btn-danger{background:#6b1c2a;color:#e94560}
.btn-danger:hover{background:#8b2438}
.btn-secondary{background:#0f3460;color:#e0e0e0}
.btn-secondary:hover{background:#1a4a7a}
.btn-sm{padding:5px 10px;font-size:.8em}
.mapping-row{display:flex;gap:8px;align-items:center;padding:10px 0;border-bottom:1px solid #0a0a1a;flex-wrap:wrap}
.mapping-row:last-child{border-bottom:none}
.mapping-osc{color:#00ff88;font-family:monospace;font-size:.95em;min-width:180px}
.mapping-pattern{color:#e94560;min-width:100px}
.mapping-color{width:20px;height:20px;border-radius:50%;display:inline-block;vertical-align:middle}
.mapping-actions{margin-left:auto;display:flex;gap:6px}
.checkbox-row{display:flex;align-items:center;gap:8px;margin:8px 0}
.checkbox-row input[type="checkbox"]{accent-color:#e94560;width:16px;height:16px}
.status-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:12px}
.status-item{background:#0a0a1a;padding:12px;border-radius:6px}
.status-item .label{font-size:.75em;color:#888;text-transform:uppercase;margin-bottom:4px}
.status-item .value{font-size:1.1em;color:#00ff88;font-family:monospace}
.toast{position:fixed;bottom:20px;right:20px;background:#0f3460;color:#e0e0e0;padding:12px 20px;border-radius:6px;border-left:3px solid #00ff88;transform:translateY(100px);opacity:0;transition:.3s;z-index:100}
.toast.show{transform:translateY(0);opacity:1}
.toast.error{border-left-color:#e94560}
.empty{text-align:center;padding:40px;color:#555}
</style>
</head>
<body>

<div class="header">
  <div class="dot"></div>
  <h1>LED Notifier</h1>
</div>

<div class="tabs">
  <button class="tab active" onclick="showTab('mappings')">Mappings</button>
  <button class="tab" onclick="showTab('settings')">Settings</button>
  <button class="tab" onclick="showTab('test')">Test</button>
  <button class="tab" onclick="showTab('status')">Status</button>
</div>

<div class="content">

  <!-- MAPPINGS TAB -->
  <div id="tab-mappings" class="panel active">
    <div class="card">
      <h3>OSC Mappings</h3>
      <div id="mapping-list"></div>
      <div style="margin-top:12px">
        <button class="btn btn-primary" onclick="showAddMapping()">+ Add Mapping</button>
      </div>
    </div>

    <div id="mapping-form" class="card" style="display:none">
      <h3 id="form-title">Add Mapping</h3>
      <input type="hidden" id="edit-index" value="-1">
      <label>OSC Address</label>
      <input type="text" id="f-osc" placeholder="/my/address">
      <div class="row">
        <div>
          <label>Pattern</label>
          <select id="f-pattern"></select>
        </div>
        <div>
          <label>Color</label>
          <input type="color" id="f-color" value="#ff0000">
        </div>
      </div>
      <div class="row">
        <div>
          <label>Brightness (<span id="f-bri-val">255</span>)</label>
          <input type="range" id="f-brightness" min="0" max="255" value="255" oninput="document.getElementById('f-bri-val').textContent=this.value">
        </div>
        <div>
          <label>Speed (<span id="f-spd-val">1.0</span>x)</label>
          <input type="range" id="f-speed" min="1" max="50" value="10" oninput="document.getElementById('f-spd-val').textContent=(this.value/10).toFixed(1)">
        </div>
      </div>
      <label>Rings</label>
      <div class="checkbox-row">
        <input type="checkbox" id="f-ring-outer" checked>
        <label for="f-ring-outer" style="display:inline;margin:0">Outer (24)</label>
        <input type="checkbox" id="f-ring-middle" checked style="margin-left:16px">
        <label for="f-ring-middle" style="display:inline;margin:0">Middle (16)</label>
        <input type="checkbox" id="f-ring-inner" checked style="margin-left:16px">
        <label for="f-ring-inner" style="display:inline;margin:0">Inner (7)</label>
      </div>
      <div class="checkbox-row">
        <input type="checkbox" id="f-useOscColor">
        <label for="f-useOscColor" style="display:inline;margin:0">Override color from OSC args (3 ints: R, G, B)</label>
      </div>
      <div class="checkbox-row">
        <input type="checkbox" id="f-useOscBrightness">
        <label for="f-useOscBrightness" style="display:inline;margin:0">Override brightness from OSC arg (float 0.0-1.0)</label>
      </div>
      <div style="margin-top:12px;display:flex;gap:8px">
        <button class="btn btn-primary" onclick="saveMapping()">Save</button>
        <button class="btn btn-secondary" onclick="hideForm()">Cancel</button>
      </div>
    </div>
  </div>

  <!-- SETTINGS TAB -->
  <div id="tab-settings" class="panel">
    <div class="card">
      <h3>Device Settings</h3>
      <label>Device Name</label>
      <input type="text" id="s-name" placeholder="led-notifier">
      <label>OSC Port</label>
      <input type="number" id="s-port" min="1" max="65535" value="9000">
      <label>Global Brightness (<span id="s-bri-val">128</span>)</label>
      <input type="range" id="s-brightness" min="0" max="255" value="128" oninput="document.getElementById('s-bri-val').textContent=this.value">
      <div class="checkbox-row" style="margin-top:12px">
        <input type="checkbox" id="s-ota">
        <label for="s-ota" style="display:inline;margin:0">Enable OTA firmware updates (requires reboot)</label>
      </div>
      <div style="margin-top:16px;display:flex;gap:8px">
        <button class="btn btn-primary" onclick="saveSettings()">Save Settings</button>
        <button class="btn btn-danger" onclick="rebootDevice()">Reboot</button>
      </div>
    </div>

    <div class="card">
      <h3>Ring Calibration</h3>
      <p style="font-size:.85em;color:#888;margin-bottom:8px">Rotate each ring so patterns align. Play a chase or rainbow while adjusting.</p>
      <label>Outer ring offset (<span id="s-off0-val">0</span> / 24)</label>
      <input type="range" id="s-off0" min="0" max="23" value="0" oninput="document.getElementById('s-off0-val').textContent=this.value;liveCalibrate()">
      <div class="checkbox-row">
        <input type="checkbox" id="s-rev0" onchange="liveCalibrate()">
        <label for="s-rev0" style="display:inline;margin:0">Reverse outer direction</label>
      </div>
      <label>Middle ring offset (<span id="s-off1-val">0</span> / 16)</label>
      <input type="range" id="s-off1" min="0" max="15" value="0" oninput="document.getElementById('s-off1-val').textContent=this.value;liveCalibrate()">
      <div class="checkbox-row">
        <input type="checkbox" id="s-rev1" onchange="liveCalibrate()">
        <label for="s-rev1" style="display:inline;margin:0">Reverse middle direction</label>
      </div>
      <label>Inner ring offset (<span id="s-off2-val">0</span> / 7)</label>
      <input type="range" id="s-off2" min="0" max="6" value="0" oninput="document.getElementById('s-off2-val').textContent=this.value;liveCalibrate()">
      <div class="checkbox-row">
        <input type="checkbox" id="s-rev2" onchange="liveCalibrate()">
        <label for="s-rev2" style="display:inline;margin:0">Reverse inner direction</label>
      </div>
      <div style="margin-top:12px;display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn btn-secondary" onclick="calibratePreview('chase')">Preview Chase</button>
        <button class="btn btn-secondary" onclick="calibratePreview('rainbow')">Preview Rainbow</button>
        <button class="btn btn-danger btn-sm" onclick="stopPattern()">Stop</button>
      </div>
    </div>

    <div class="card">
      <h3>Hardware</h3>
      <label>WS2812B data pin (GPIO)</label>
      <select id="s-ledpin">
        <option value="2">GPIO 2</option>
        <option value="4">GPIO 4</option>
        <option value="5">GPIO 5</option>
        <option value="12">GPIO 12</option>
        <option value="13">GPIO 13</option>
        <option value="14">GPIO 14</option>
        <option value="15">GPIO 15</option>
        <option value="16">GPIO 16</option>
        <option value="17">GPIO 17</option>
        <option value="18">GPIO 18</option>
        <option value="19">GPIO 19</option>
        <option value="21">GPIO 21</option>
        <option value="22">GPIO 22</option>
        <option value="23">GPIO 23</option>
        <option value="25">GPIO 25</option>
        <option value="26">GPIO 26</option>
        <option value="27">GPIO 27</option>
        <option value="32">GPIO 32</option>
        <option value="33">GPIO 33</option>
      </select>
      <p style="font-size:.8em;color:#888;margin-top:6px">Change requires a reboot. On WT32-ETH01 avoid GPIOs 0, 16-19, 21-23, 25-27 (used by Ethernet).</p>
    </div>

    <div class="card">
      <h3>sACN / E1.31 (DMX over IP)</h3>
      <div class="checkbox-row">
        <input type="checkbox" id="s-sacn-en">
        <label for="s-sacn-en" style="display:inline;margin:0">Enable sACN receive</label>
      </div>
      <div class="row">
        <div>
          <label>Universe (1-63999)</label>
          <input type="number" id="s-sacn-univ" min="1" max="63999" value="1">
        </div>
        <div>
          <label>Start address (1-512)</label>
          <input type="number" id="s-sacn-start" min="1" max="512" value="1">
        </div>
        <div>
          <label>Priority (0-200)</label>
          <input type="number" id="s-sacn-prio" min="0" max="200" value="100">
        </div>
      </div>
      <div class="checkbox-row">
        <input type="checkbox" id="s-sacn-mcast" checked>
        <label for="s-sacn-mcast" style="display:inline;margin:0">Multicast (uncheck for unicast)</label>
      </div>
      <p style="font-size:.85em;color:#888;margin-top:6px">
        Multicast address: <span id="s-sacn-addr" style="color:#00ff88;font-family:monospace">—</span><br>
        Channels used: <span id="s-sacn-chans" style="color:#00ff88;font-family:monospace">—</span>
      </p>
    </div>

    <div class="card">
      <h3>Backup & Restore</h3>
      <div style="display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn btn-secondary" onclick="exportConfig()">Export Config</button>
        <button class="btn btn-secondary" onclick="document.getElementById('import-file').click()">Import Config</button>
        <input type="file" id="import-file" accept="application/json,.json" style="display:none" onchange="importConfig(event)">
      </div>
    </div>
  </div>

  <!-- TEST TAB -->
  <div id="tab-test" class="panel">
    <div class="card">
      <h3>Test Patterns</h3>
      <div class="row">
        <div>
          <label>Pattern</label>
          <select id="t-pattern"></select>
        </div>
        <div>
          <label>Color</label>
          <input type="color" id="t-color" value="#ff0000">
        </div>
      </div>
      <div class="row">
        <div>
          <label>Brightness (<span id="t-bri-val">200</span>)</label>
          <input type="range" id="t-brightness" min="0" max="255" value="200" oninput="document.getElementById('t-bri-val').textContent=this.value">
        </div>
        <div>
          <label>Speed (<span id="t-spd-val">1.0</span>x)</label>
          <input type="range" id="t-speed" min="1" max="50" value="10" oninput="document.getElementById('t-spd-val').textContent=(this.value/10).toFixed(1)">
        </div>
      </div>
      <label>Rings</label>
      <div class="checkbox-row">
        <input type="checkbox" id="t-ring-outer" checked>
        <label for="t-ring-outer" style="display:inline;margin:0">Outer (24)</label>
        <input type="checkbox" id="t-ring-middle" checked style="margin-left:16px">
        <label for="t-ring-middle" style="display:inline;margin:0">Middle (16)</label>
        <input type="checkbox" id="t-ring-inner" checked style="margin-left:16px">
        <label for="t-ring-inner" style="display:inline;margin:0">Inner (7)</label>
      </div>
      <div style="margin-top:12px;display:flex;gap:8px">
        <button class="btn btn-primary" onclick="testPattern()">Play</button>
        <button class="btn btn-danger" onclick="stopPattern()">Stop</button>
      </div>
    </div>
  </div>

  <!-- STATUS TAB -->
  <div id="tab-status" class="panel">
    <div class="card">
      <h3>Device Status</h3>
      <div class="status-grid" id="status-grid">
        <div class="status-item"><div class="label">Loading...</div><div class="value">-</div></div>
      </div>
      <div style="margin-top:12px">
        <button class="btn btn-secondary" onclick="loadStatus()">Refresh</button>
      </div>
    </div>
  </div>

</div>

<div id="toast" class="toast"></div>

<script>
let config = { globalBrightness: 128, oscPort: 9000, deviceName: 'led-notifier', mappings: [] };
let patterns = [];

// --- Tabs ---
function showTab(name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelector(`[onclick="showTab('${name}')"]`).classList.add('active');
  document.getElementById('tab-' + name).classList.add('active');
  if (name === 'status') loadStatus();
}

// --- Toast ---
function toast(msg, error) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast show' + (error ? ' error' : '');
  setTimeout(() => t.className = 'toast', 3000);
}

// --- API helpers ---
async function api(method, path, body) {
  const opts = { method, headers: { 'Content-Type': 'application/json' } };
  if (body) opts.body = JSON.stringify(body);
  const r = await fetch(path, opts);
  if (!r.ok) throw new Error(r.statusText);
  return r.json().catch(() => ({}));
}

// --- Load data ---
async function loadConfig() {
  try {
    config = await api('GET', '/api/config');
    renderMappings();
    renderSettings();
  } catch (e) { toast('Failed to load config', true); }
}

async function loadPatterns() {
  try {
    patterns = await api('GET', '/api/patterns');
    populatePatternDropdowns();
  } catch (e) { toast('Failed to load patterns', true); }
}

function populatePatternDropdowns() {
  ['f-pattern', 't-pattern'].forEach(id => {
    const sel = document.getElementById(id);
    sel.innerHTML = '';
    patterns.forEach(p => {
      const opt = document.createElement('option');
      opt.value = p.name;
      opt.textContent = p.displayName + (p.hasCountdown ? ' ⏱' : '');
      sel.appendChild(opt);
    });
  });
}

// --- Mappings ---
function renderMappings() {
  const list = document.getElementById('mapping-list');
  if (!config.mappings || config.mappings.length === 0) {
    list.innerHTML = '<div class="empty">No mappings configured. Add one to get started.</div>';
    return;
  }
  list.innerHTML = config.mappings.map((m, i) => `
    <div class="mapping-row">
      <span class="mapping-osc">${escHtml(m.osc)}</span>
      <span class="mapping-pattern">${escHtml(m.pattern)}</span>
      <span class="mapping-color" style="background:#${m.color}"></span>
      <span style="font-size:.8em;color:#888">bri:${m.brightness} spd:${m.speed} rings:${ringMaskLabel(m.ringMask == null ? 7 : m.ringMask)}</span>
      <div class="mapping-actions">
        <button class="btn btn-secondary btn-sm" onclick="editMapping(${i})">Edit</button>
        <button class="btn btn-danger btn-sm" onclick="deleteMapping(${i})">Del</button>
      </div>
    </div>
  `).join('');
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function setRingChecks(prefix, mask) {
  document.getElementById(prefix + '-ring-outer').checked = !!(mask & 1);
  document.getElementById(prefix + '-ring-middle').checked = !!(mask & 2);
  document.getElementById(prefix + '-ring-inner').checked = !!(mask & 4);
}

function getRingMask(prefix) {
  let m = 0;
  if (document.getElementById(prefix + '-ring-outer').checked) m |= 1;
  if (document.getElementById(prefix + '-ring-middle').checked) m |= 2;
  if (document.getElementById(prefix + '-ring-inner').checked) m |= 4;
  return m || 7;  // never allow 0 (all off)
}

function ringMaskLabel(mask) {
  if ((mask & 7) === 7) return 'all';
  const parts = [];
  if (mask & 1) parts.push('O');
  if (mask & 2) parts.push('M');
  if (mask & 4) parts.push('I');
  return parts.join('+') || 'none';
}

function showAddMapping() {
  document.getElementById('form-title').textContent = 'Add Mapping';
  document.getElementById('edit-index').value = -1;
  document.getElementById('f-osc').value = '/';
  document.getElementById('f-pattern').value = patterns.length ? patterns[0].name : '';
  document.getElementById('f-color').value = '#ff0000';
  document.getElementById('f-brightness').value = 255;
  document.getElementById('f-bri-val').textContent = '255';
  document.getElementById('f-speed').value = 10;
  document.getElementById('f-spd-val').textContent = '1.0';
  setRingChecks('f', 7);
  document.getElementById('f-useOscColor').checked = false;
  document.getElementById('f-useOscBrightness').checked = false;
  document.getElementById('mapping-form').style.display = 'block';
}

function editMapping(idx) {
  const m = config.mappings[idx];
  document.getElementById('form-title').textContent = 'Edit Mapping';
  document.getElementById('edit-index').value = idx;
  document.getElementById('f-osc').value = m.osc;
  document.getElementById('f-pattern').value = m.pattern;
  document.getElementById('f-color').value = '#' + m.color;
  document.getElementById('f-brightness').value = m.brightness;
  document.getElementById('f-bri-val').textContent = m.brightness;
  document.getElementById('f-speed').value = Math.round(m.speed * 10);
  document.getElementById('f-spd-val').textContent = m.speed.toFixed(1);
  setRingChecks('f', (m.ringMask == null ? 7 : m.ringMask));
  document.getElementById('f-useOscColor').checked = m.useOscColor || false;
  document.getElementById('f-useOscBrightness').checked = m.useOscBrightness || false;
  document.getElementById('mapping-form').style.display = 'block';
}

function hideForm() {
  document.getElementById('mapping-form').style.display = 'none';
}

async function saveMapping() {
  const idx = parseInt(document.getElementById('edit-index').value);
  const mapping = {
    osc: document.getElementById('f-osc').value.trim(),
    pattern: document.getElementById('f-pattern').value,
    color: document.getElementById('f-color').value.substring(1).toUpperCase(),
    brightness: parseInt(document.getElementById('f-brightness').value),
    speed: parseInt(document.getElementById('f-speed').value) / 10,
    ringMask: getRingMask('f'),
    useOscColor: document.getElementById('f-useOscColor').checked,
    useOscBrightness: document.getElementById('f-useOscBrightness').checked
  };
  if (!mapping.osc || !mapping.osc.startsWith('/')) {
    toast('OSC address must start with /', true);
    return;
  }
  try {
    if (idx >= 0) {
      config.mappings[idx] = mapping;
    } else {
      config.mappings.push(mapping);
    }
    await api('PUT', '/api/config', config);
    toast('Mapping saved');
    hideForm();
    renderMappings();
  } catch (e) { toast('Failed to save', true); }
}

async function deleteMapping(idx) {
  if (!confirm('Delete this mapping?')) return;
  try {
    config.mappings.splice(idx, 1);
    await api('PUT', '/api/config', config);
    toast('Mapping deleted');
    renderMappings();
  } catch (e) { toast('Failed to delete', true); }
}

// --- Settings ---
function renderSettings() {
  document.getElementById('s-name').value = config.deviceName || 'led-notifier';
  document.getElementById('s-port').value = config.oscPort || 9000;
  document.getElementById('s-brightness').value = config.globalBrightness || 128;
  document.getElementById('s-bri-val').textContent = config.globalBrightness || 128;
  document.getElementById('s-ota').checked = !!config.otaEnabled;
  const offs = config.ringOffset || [0, 0, 0];
  const revs = config.ringReverse || [false, false, false];
  for (let i = 0; i < 3; i++) {
    document.getElementById('s-off' + i).value = offs[i] || 0;
    document.getElementById('s-off' + i + '-val').textContent = offs[i] || 0;
    document.getElementById('s-rev' + i).checked = !!revs[i];
  }
  if (config.ledPin != null) document.getElementById('s-ledpin').value = String(config.ledPin);
  document.getElementById('s-sacn-en').checked = !!config.sacnEnabled;
  document.getElementById('s-sacn-univ').value = config.sacnUniverse || 1;
  document.getElementById('s-sacn-start').value = config.sacnStartAddr || 1;
  document.getElementById('s-sacn-prio').value = config.sacnPriority != null ? config.sacnPriority : 100;
  document.getElementById('s-sacn-mcast').checked = (config.sacnMulticast !== false);
  updateSacnDisplay();
}

function computeMcastAddr(univ) {
  univ = parseInt(univ) || 1;
  return '239.255.' + ((univ >> 8) & 0xFF) + '.' + (univ & 0xFF);
}
function updateSacnDisplay() {
  const univ = document.getElementById('s-sacn-univ').value;
  const start = parseInt(document.getElementById('s-sacn-start').value) || 1;
  const mc = document.getElementById('s-sacn-mcast').checked;
  document.getElementById('s-sacn-addr').textContent = mc ? computeMcastAddr(univ) : 'unicast (port 5568)';
  const need = 47 * 3;
  const end = start + need - 1;
  document.getElementById('s-sacn-chans').textContent =
    end > 512
      ? `${start}-${end}  ⚠ overflows 512`
      : `${start}-${end}  (${need} channels)`;
}
document.addEventListener('DOMContentLoaded', () => {
  ['s-sacn-univ','s-sacn-start','s-sacn-mcast'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.addEventListener('input', updateSacnDisplay);
  });
});

async function saveSettings() {
  config.deviceName = document.getElementById('s-name').value.trim();
  config.oscPort = parseInt(document.getElementById('s-port').value);
  config.globalBrightness = parseInt(document.getElementById('s-brightness').value);
  config.otaEnabled = document.getElementById('s-ota').checked;
  config.ringOffset = [
    parseInt(document.getElementById('s-off0').value),
    parseInt(document.getElementById('s-off1').value),
    parseInt(document.getElementById('s-off2').value)
  ];
  config.ringReverse = [
    document.getElementById('s-rev0').checked,
    document.getElementById('s-rev1').checked,
    document.getElementById('s-rev2').checked
  ];
  config.ledPin = parseInt(document.getElementById('s-ledpin').value);
  config.sacnEnabled = document.getElementById('s-sacn-en').checked;
  config.sacnUniverse = parseInt(document.getElementById('s-sacn-univ').value);
  config.sacnStartAddr = parseInt(document.getElementById('s-sacn-start').value);
  config.sacnPriority = parseInt(document.getElementById('s-sacn-prio').value);
  config.sacnMulticast = document.getElementById('s-sacn-mcast').checked;
  try {
    const r = await api('PUT', '/api/config', config);
    toast(r.rebootRequired ? 'Saved — reboot to apply pin/OTA change' : 'Settings saved');
  } catch (e) { toast('Failed to save', true); }
}

// --- Calibration ---
let lastPreview = null;
let calibrateTimer = null;
async function calibratePreview(name) {
  lastPreview = name;
  const body = name === 'chase'
    ? { pattern: 'chase', color: 'FF00FF', brightness: 180, speed: 0.5, ringMask: 7 }
    : { pattern: 'rainbow', color: 'FFFFFF', brightness: 180, speed: 0.5, ringMask: 7 };
  try { await api('POST', '/api/test', body); } catch (e) {}
}
function liveCalibrate() {
  // Debounce: push offsets to device as slider moves
  clearTimeout(calibrateTimer);
  calibrateTimer = setTimeout(async () => {
    config.ringOffset = [
      parseInt(document.getElementById('s-off0').value),
      parseInt(document.getElementById('s-off1').value),
      parseInt(document.getElementById('s-off2').value)
    ];
    config.ringReverse = [
      document.getElementById('s-rev0').checked,
      document.getElementById('s-rev1').checked,
      document.getElementById('s-rev2').checked
    ];
    try {
      await api('PUT', '/api/config', config);
      if (lastPreview) await calibratePreview(lastPreview);
    } catch (e) {}
  }, 150);
}

// --- Export / Import ---
async function exportConfig() {
  try {
    const r = await fetch('/api/config/export');
    const text = await r.text();
    const blob = new Blob([text], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = (config.deviceName || 'led-notifier') + '-config.json';
    a.click();
    URL.revokeObjectURL(a.href);
    toast('Exported');
  } catch (e) { toast('Export failed', true); }
}
function importConfig(ev) {
  const file = ev.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = async () => {
    try {
      const imported = JSON.parse(reader.result);
      if (!confirm('Replace current config with imported file?')) return;
      await api('PUT', '/api/config', imported);
      toast('Imported — reloading');
      setTimeout(() => location.reload(), 600);
    } catch (e) { toast('Invalid config file', true); }
  };
  reader.readAsText(file);
  ev.target.value = '';
}

async function rebootDevice() {
  if (!confirm('Reboot the device?')) return;
  try {
    await api('POST', '/api/reboot');
    toast('Rebooting...');
  } catch (e) {}
}

// --- Test ---
async function testPattern() {
  const body = {
    pattern: document.getElementById('t-pattern').value,
    color: document.getElementById('t-color').value.substring(1).toUpperCase(),
    brightness: parseInt(document.getElementById('t-brightness').value),
    speed: parseInt(document.getElementById('t-speed').value) / 10,
    ringMask: getRingMask('t')
  };
  try {
    await api('POST', '/api/test', body);
    toast('Pattern triggered');
  } catch (e) { toast('Failed', true); }
}

async function stopPattern() {
  try {
    await api('POST', '/api/stop');
    toast('Stopped');
  } catch (e) { toast('Failed to stop', true); }
}

// --- Status ---
async function loadStatus() {
  try {
    const s = await api('GET', '/api/status');
    const grid = document.getElementById('status-grid');
    grid.innerHTML = [
      { label: 'Network', value: (s.network || 'wifi').toUpperCase() },
      { label: 'IP Address', value: s.ip || '-' },
      { label: 'Device Name', value: s.deviceName || '-' },
      { label: 'Uptime', value: formatUptime(s.uptime || 0) },
      { label: 'Free Heap', value: (s.freeHeap || 0).toLocaleString() + ' bytes' },
      { label: 'WiFi RSSI', value: (s.network === 'wifi' ? (s.rssi || 0) + ' dBm' : 'n/a') },
      { label: 'OSC Port', value: s.oscPort || '-' },
      { label: 'Active Pattern', value: s.activePattern || 'None' },
      { label: 'LED Count', value: s.numLeds || '-' },
      { label: 'LED Pin', value: 'GPIO ' + (s.ledPin != null ? s.ledPin : '-') },
      { label: 'sACN', value: s.sacnEnabled ? (s.sacnActive ? 'Receiving' : 'Listening') : 'Disabled' },
    ].map(i => `<div class="status-item"><div class="label">${i.label}</div><div class="value">${i.value}</div></div>`).join('');
  } catch (e) { toast('Failed to load status', true); }
}

function formatUptime(ms) {
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
  if (h > 0) return h + 'h ' + m + 'm';
  return m + 'm ' + (s % 60) + 's';
}

// --- Init ---
window.addEventListener('DOMContentLoaded', () => {
  loadPatterns().then(loadConfig);
});
</script>
</body>
</html>
)rawliteral";

#endif
