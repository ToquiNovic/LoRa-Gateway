#ifndef GATEWAY_HTML_H
#define GATEWAY_HTML_H

#include <pgmspace.h>

// ====================================================================
// GATEWAY WEB UI - Real-Time Sensor Inspector & Management Portal
// ====================================================================

const char GATEWAY_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Betty IoT Gateway | Sensor Inspector</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #38bdf8;
      --primary-hover: #0284c7;
      --success: #22c55e;
      --warning: #f59e0b;
      --danger: #ef4444;
      --border: #334155;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
      padding: 1.5rem;
      line-height: 1.5;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
      margin-bottom: 1.5rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid var(--border);
    }
    .header h1 { font-size: 1.5rem; font-weight: 700; color: var(--primary); }
    .badge {
      display: inline-block;
      padding: 0.25rem 0.6rem;
      border-radius: 9999px;
      font-size: 0.75rem;
      font-weight: 600;
      text-transform: uppercase;
    }
    .badge-success { background: rgba(34, 197, 94, 0.2); color: var(--success); }
    .badge-warning { background: rgba(245, 158, 11, 0.2); color: var(--warning); }
    .badge-danger { background: rgba(239, 68, 68, 0.2); color: var(--danger); }
    .badge-info { background: rgba(56, 189, 248, 0.2); color: var(--primary); }
    
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 1rem;
      margin-bottom: 1.5rem;
    }
    .stat-card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 0.5rem;
      padding: 1rem;
    }
    .stat-title { font-size: 0.8rem; color: var(--text-muted); text-transform: uppercase; }
    .stat-value { font-size: 1.5rem; font-weight: 700; margin-top: 0.25rem; }

    .btn {
      background: var(--primary);
      color: #0f172a;
      padding: 0.5rem 1rem;
      border-radius: 0.375rem;
      font-weight: 600;
      border: none;
      cursor: pointer;
      transition: background 0.2s;
    }
    .btn:hover { background: var(--primary-hover); }
    .btn-secondary { background: var(--border); color: var(--text); }
    .btn-secondary:hover { background: #475569; }

    .card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 0.5rem;
      margin-bottom: 1.5rem;
      overflow: hidden;
    }
    .card-header {
      padding: 1rem 1.25rem;
      border-bottom: 1px solid var(--border);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .card-header h2 { font-size: 1.1rem; }

    table { width: 100%; border-collapse: collapse; text-align: left; }
    th, td { padding: 0.75rem 1.25rem; border-bottom: 1px solid var(--border); }
    th { background: rgba(15, 23, 42, 0.6); color: var(--text-muted); font-size: 0.8rem; text-transform: uppercase; }
    tr:hover { background: rgba(255, 255, 255, 0.02); }

    .metrics-tag {
      background: rgba(15, 23, 42, 0.8);
      padding: 0.25rem 0.5rem;
      border-radius: 0.25rem;
      font-family: monospace;
      font-size: 0.8rem;
      color: #38bdf8;
      display: inline-block;
      max-width: 320px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .form-group { margin-bottom: 1rem; }
    .form-group label { display: block; font-size: 0.85rem; margin-bottom: 0.25rem; color: var(--text-muted); }
    .form-group input, .form-group select {
      width: 100%;
      background: var(--bg);
      border: 1px solid var(--border);
      color: var(--text);
      padding: 0.5rem;
      border-radius: 0.25rem;
      font-size: 0.9rem;
    }
    .form-row { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
    @media (max-width: 600px) {
      .form-row { grid-template-columns: 1fr; gap: 0.5rem; }
    }

    .network-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.5rem 0.75rem;
      border-radius: 0.25rem;
      cursor: pointer;
      transition: background 0.15s;
      border-bottom: 1px solid var(--border);
      font-size: 0.85rem;
    }
    .network-item:last-child { border-bottom: none; }
    .network-item:hover, .network-item:active {
      background: rgba(56, 189, 248, 0.15);
      color: var(--primary);
    }
    .alert-box {
      padding: 0.75rem 1rem;
      border-radius: 0.25rem;
      font-size: 0.85rem;
      margin-bottom: 1rem;
    }
    .alert-info { background: rgba(56, 189, 248, 0.15); border: 1px solid var(--primary); color: var(--primary); }
    .alert-success { background: rgba(34, 197, 94, 0.15); border: 1px solid var(--success); color: var(--success); }
    .alert-danger { background: rgba(239, 68, 68, 0.15); border: 1px solid var(--danger); color: var(--danger); }
  </style>
</head>
<body>

  <div class="header">
    <div>
      <h1>🌐 Betty IoT Gateway (Nodo Base)</h1>
      <p style="color: var(--text-muted); font-size: 0.9rem;">Authenticated Multi-Transport Sensor Bridge & Inspector</p>
    </div>
    <div style="display: flex; gap: 0.5rem; flex-wrap: wrap;">
      <button class="btn btn-secondary" onclick="toggleOta()">🚀 OTA Update</button>
      <button class="btn btn-secondary" onclick="toggleConfig()">⚙️ Gateway Settings</button>
      <button class="btn" onclick="startPairing()">🔐 Pair Sensor (60s WPS)</button>
    </div>
  </div>

  <div class="stats-grid">
    <div class="stat-card">
      <div class="stat-title">Connected Sensors</div>
      <div class="stat-value" id="stat-sensors">0 / 0</div>
    </div>
    <div class="stat-card">
      <div class="stat-title">Betty PaaS Uplink</div>
      <div class="stat-value" id="stat-betty">
        <span class="badge badge-warning" id="betty-status-badge">Standby</span>
      </div>
    </div>
    <div class="stat-card">
      <div class="stat-title">Dispatched Telemetry</div>
      <div class="stat-value" id="stat-sent">0</div>
    </div>
    <div class="stat-card">
      <div class="stat-title">Security Drops (Auth)</div>
      <div class="stat-value" style="color: var(--danger);" id="stat-drops">0</div>
    </div>
  </div>

  <!-- Settings Panel (Hidden by default) -->
  <div class="card" id="config-panel" style="display: none;">
    <div class="card-header">
      <h2>⚙️ Conectar Gateway a Red Wi-Fi</h2>
      <button type="button" class="btn btn-secondary" onclick="toggleConfig()">Cerrar</button>
    </div>
    <div style="padding: 1.25rem;">
      <!-- Inline Status Alert -->
      <div id="cfg-status-box" style="display: none;" class="alert-box"></div>

      <!-- Nearby Wi-Fi Networks Selector -->
      <div style="margin-bottom: 1.25rem;">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
          <label style="font-weight: 600; font-size: 0.9rem; color: var(--primary);">Redes Wi-Fi Cercanas</label>
          <button type="button" id="btn-scan" class="btn btn-secondary" style="padding: 0.35rem 0.8rem; font-size: 0.8rem;" onclick="startScan()">🔍 Buscar Redes</button>
        </div>
        <div id="networks-list" style="max-height: 150px; overflow-y: auto; background: var(--bg); border: 1px solid var(--border); border-radius: 0.35rem; padding: 0.25rem;">
          <div style="text-align: center; color: var(--text-muted); font-size: 0.85rem; padding: 0.75rem;">
            Toca <strong>"🔍 Buscar Redes"</strong> para detectar redes cercanas o escribe el nombre abajo.
          </div>
        </div>
      </div>

      <!-- Network Credentials Inputs (Standard div container, NO native form submission) -->
      <div class="form-row">
        <div class="form-group">
          <label>Nombre de Red Wi-Fi (SSID)</label>
          <input type="text" id="cfg-ssid" placeholder="Selecciona arriba o escribe el SSID">
        </div>
        <div class="form-group">
          <label>Contraseña Wi-Fi</label>
          <input type="password" id="cfg-pass" placeholder="••••••••">
        </div>
      </div>

      <div class="form-row">
        <div class="form-group">
          <label>Clave de Enlace (PSK para Sensores)</label>
          <input type="text" id="cfg-psk" value="betty1234" placeholder="betty1234">
          <small style="color: var(--text-muted); font-size: 0.75rem;">Clave que deben enviar los sensores para que el Gateway acepte sus paquetes.</small>
        </div>
        <div class="form-group" style="display: flex; flex-direction: column; justify-content: flex-end;">
          <button type="button" id="btn-save" class="btn" style="height: 42px; font-weight: 700; width: 100%;" onclick="saveWifiConfig()">💾 Guardar y Conectar a Wi-Fi</button>
        </div>
      </div>

      <div class="form-row">
        <div class="form-group">
          <label>ID del Gateway (Firma de Antena)</label>
          <input type="text" id="cfg-gateway-id" placeholder="gateway-esp32-01">
          <small style="color: var(--text-muted); font-size: 0.75rem;">Identificador enviado a Botuto en el header <code>X-Gateway-ID</code>.</small>
        </div>
        <div class="form-group">
          <label>API Key de Botuto (Cloud Secret)</label>
          <input type="password" id="cfg-api-key" placeholder="betty_live_secret">
          <small style="color: var(--text-muted); font-size: 0.75rem;">Token de autenticación enviado en el header <code>X-API-Key</code>.</small>
        </div>
      </div>

      <!-- Informational Bridge Note -->
      <div style="margin-top: 1rem; border-top: 1px solid var(--border); padding-top: 0.75rem; color: var(--text-muted); font-size: 0.8rem;">
        <span>🌐 <strong>Enlace Transparente a Betty/Botuto:</strong> Los datos de cada sensor recibido (LoRa, Wi-Fi, WebSockets o Serie) se reenvían automáticamente a Betty con sus variables y su propio identificador.</span>
      </div>
    </div>
  </div>

  <!-- OTA Firmware Update Panel (Hidden by default) -->
  <div class="card" id="ota-panel" style="display: none;">
    <div class="card-header">
      <h2>🚀 Actualización Remota de Firmware (OTA)</h2>
      <button type="button" class="btn btn-secondary" onclick="toggleOta()">Cerrar</button>
    </div>
    <div style="padding: 1.25rem;">
      <div id="ota-status-box" style="display: none;" class="alert-box"></div>
      <p style="color: var(--text-muted); font-size: 0.85rem; margin-bottom: 1rem;">
        Sube el archivo binario compilado (<strong>firmware.bin</strong>) para actualizar el Gateway de forma 100% remota sin desconectar la antena.
      </p>

      <div class="form-group">
        <label>Seleccionar Archivo de Firmware (.bin)</label>
        <input type="file" id="ota-file" accept=".bin" style="padding: 0.4rem; background: var(--bg); border: 1px solid var(--border); width: 100%; border-radius: 0.25rem; color: var(--text);">
      </div>

      <div id="ota-progress-container" style="display: none; margin-bottom: 1rem;">
        <div style="display: flex; justify-content: space-between; font-size: 0.8rem; margin-bottom: 0.25rem;">
          <span id="ota-progress-text">Subiendo firmware...</span>
          <span id="ota-progress-percent">0%</span>
        </div>
        <div style="width: 100%; background: var(--bg); height: 10px; border-radius: 5px; overflow: hidden; border: 1px solid var(--border);">
          <div id="ota-progress-bar" style="width: 0%; height: 100%; background: var(--primary); transition: width 0.2s;"></div>
        </div>
      </div>

      <button type="button" id="btn-ota-upload" class="btn" style="height: 42px; font-weight: 700; width: 100%;" onclick="uploadFirmware()">
        ⚡ Iniciar Actualización de Firmware
      </button>
    </div>
  </div>

  <!-- Connected Sensors Table -->
  <div class="card">
    <div class="card-header">
      <h2>🔍 Live Connected Sensors</h2>
      <span id="pairing-timer" style="color: var(--warning); font-size: 0.85rem; font-weight: 600;"></span>
    </div>
    <table>
      <thead>
        <tr>
          <th>Sensor ID</th>
          <th>Interface</th>
          <th>Model</th>
          <th>Signal (RSSI)</th>
          <th>Packets</th>
          <th>Last Seen</th>
          <th>Latest Metrics</th>
        </tr>
      </thead>
      <tbody id="sensors-table">
        <tr><td colspan="7" style="text-align: center; color: var(--text-muted);">Listening for sensor beacons...</td></tr>
      </tbody>
    </table>
  </div>

  <script>
    let ws;
    function connectWS() {
      ws = new WebSocket('ws://' + window.location.hostname + ':81');
      ws.onmessage = (e) => {
        try {
          const msg = JSON.parse(e.data);
          if (msg.type === 'sensors_update' || msg.sensors) {
            renderSensors(msg.sensors);
          } else {
            fetchSensors();
          }
        } catch (_) {}
      };
      ws.onclose = () => setTimeout(connectWS, 2500);
    }

    async function fetchSensors() {
      try {
        const res = await fetch('/api/sensors/connected');
        const data = await res.json();
        renderSensors(data.sensors || []);
        document.getElementById('stat-sensors').innerText = `${data.online || 0} / ${data.total || 0}`;
        fetchStatus();
      } catch (err) {}
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        document.getElementById('stat-sent').innerText = data.sentToBetty || 0;
        document.getElementById('stat-drops').innerText = data.authRejections || 0;
        const bBadge = document.getElementById('betty-status-badge');
        if (data.bettyConnected) {
          bBadge.className = 'badge badge-success';
          bBadge.innerText = 'ONLINE';
        } else {
          bBadge.className = 'badge badge-warning';
          bBadge.innerText = 'OFFLINE / RETRY';
        }
        if (data.pairingRemaining > 0) {
          document.getElementById('pairing-timer').innerText = `🔐 WPS Pairing Active: ${data.pairingRemaining}s remaining`;
        } else {
          document.getElementById('pairing-timer').innerText = '';
        }
        if (data.gatewayId && !document.getElementById('cfg-gateway-id').value) {
          document.getElementById('cfg-gateway-id').value = data.gatewayId;
        }
        if (data.apiKey && !document.getElementById('cfg-api-key').value) {
          document.getElementById('cfg-api-key').value = data.apiKey;
        }
      } catch (_) {}
    }

    function renderSensors(sensors) {
      const tb = document.getElementById('sensors-table');
      if (!sensors || sensors.length === 0) {
        tb.innerHTML = '<tr><td colspan="7" style="text-align: center; color: var(--text-muted);">No sensors active yet. Send data via LoRa, HTTP or WebSockets.</td></tr>';
        return;
      }
      let html = '';
      sensors.forEach(s => {
        const ifBadge = s.interface === 'lora' ? 'badge-info' : (s.interface === 'wifi_http' ? 'badge-success' : 'badge-warning');
        const statusIcon = s.online ? '🟢' : '⚪';
        const metricsStr = typeof s.metrics === 'object' ? JSON.stringify(s.metrics) : (s.raw || '{}');
        html += `
          <tr>
            <td><strong>${statusIcon} ${s.id}</strong></td>
            <td><span class="badge ${ifBadge}">${s.interface}</span></td>
            <td>${s.model || 'Unknown'}</td>
            <td>${s.rssi ? s.rssi + ' dBm' : 'N/A'}</td>
            <td>${s.packets}</td>
            <td>${s.ageSec}s ago</td>
            <td><span class="metrics-tag" title='${metricsStr}'>${metricsStr}</span></td>
          </tr>
        `;
      });
      tb.innerHTML = html;
    }

    async function startPairing() {
      await fetch('/api/sensors/pair', { method: 'POST' });
      fetchStatus();
    }

    function showStatus(msg, type) {
      const box = document.getElementById('cfg-status-box');
      box.className = 'alert-box alert-' + type;
      box.innerHTML = msg;
      box.style.display = 'block';
    }

    async function startScan() {
      const btn = document.getElementById('btn-scan');
      const list = document.getElementById('networks-list');
      btn.disabled = true;
      btn.innerText = '⏳ Buscando redes...';
      list.innerHTML = '<div style="text-align:center; color:var(--text-muted); padding:0.75rem; font-size:0.85rem;">Escaneando canales Wi-Fi 2.4 GHz...</div>';

      try {
        const res = await fetch('/api/wifi/scan');
        const data = await res.json();
        renderNetworkList(data.networks || []);
      } catch (err) {
        list.innerHTML = '<div style="text-align:center; color:var(--danger); padding:0.75rem; font-size:0.85rem;">⚠️ No se pudo completar el escaneo. Puedes escribir el SSID abajo.</div>';
      } finally {
        btn.disabled = false;
        btn.innerText = '🔄 Actualizar Redes';
      }
    }

    function renderNetworkList(networks) {
      const list = document.getElementById('networks-list');
      if (!networks || networks.length === 0) {
        list.innerHTML = '<div style="text-align:center; color:var(--text-muted); padding:0.75rem; font-size:0.85rem;">No se detectaron redes en este momento. Puedes escribir el SSID abajo.</div>';
        return;
      }

      let html = '';
      networks.forEach(n => {
        const lock = n.secure ? '🔒' : '🔓';
        const escaped = n.ssid.replace(/'/g, "\\'");
        html += `
          <div class="network-item" onclick="selectNetwork('${escaped}')">
            <span>📶 <strong>${n.ssid}</strong></span>
            <span class="badge badge-info" style="font-size:0.75rem;">${n.rssi} dBm ${lock}</span>
          </div>
        `;
      });
      list.innerHTML = html;
    }

    function selectNetwork(ssid) {
      document.getElementById('cfg-ssid').value = ssid;
      const passField = document.getElementById('cfg-pass');
      passField.focus();
      showStatus('Red seleccionada: <strong>' + ssid + '</strong>. Escribe la contraseña y presiona Guardar.', 'info');
    }

    function toggleConfig() {
      const p = document.getElementById('config-panel');
      const isOpening = (p.style.display === 'none' || p.style.display === '');
      p.style.display = isOpening ? 'block' : 'none';
      if (isOpening) {
        p.scrollIntoView({ behavior: 'smooth' });
      }
    }

    async function saveWifiConfig() {
      const ssidInput = document.getElementById('cfg-ssid');
      const passInput = document.getElementById('cfg-pass');
      const pskInput = document.getElementById('cfg-psk');
      const btnSave = document.getElementById('btn-save');

      const ssid = ssidInput.value.trim();
      const pass = passInput.value;
      const psk = pskInput.value.trim();

      if (!ssid) {
        showStatus('⚠️ Por favor selecciona una red de la lista o escribe el nombre (SSID).', 'danger');
        ssidInput.focus();
        return;
      }

      showStatus('⏳ Guardando configuración y conectando a "' + ssid + '"...', 'info');
      btnSave.disabled = true;

      try {
        const payload = {
          ssid: ssid,
          pass: pass,
          psk: psk,
          sensorId: document.getElementById('cfg-gateway-id').value.trim(),
          apiKey: document.getElementById('cfg-api-key').value.trim()
        };

        const res = await fetch('/api/gateway/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });

        if (res.ok) {
          showStatus('✅ ¡Configuración guardada exitosamente! El Gateway se está asociando a "' + ssid + '".', 'success');
        } else {
          showStatus('⚠️ El servidor respondió con error ' + res.status, 'danger');
          btnSave.disabled = false;
        }
      } catch (err) {
        showStatus('❌ Error al guardar configuración: ' + err.message, 'danger');
        btnSave.disabled = false;
      }
    }

    function toggleOta() {
      const p = document.getElementById('ota-panel');
      const isOpening = (p.style.display === 'none' || p.style.display === '');
      p.style.display = isOpening ? 'block' : 'none';
      if (isOpening) {
        p.scrollIntoView({ behavior: 'smooth' });
      }
    }

    function showOtaStatus(msg, type) {
      const box = document.getElementById('ota-status-box');
      box.className = 'alert-box alert-' + type;
      box.innerHTML = msg;
      box.style.display = 'block';
    }

    function uploadFirmware() {
      const fileInput = document.getElementById('ota-file');
      const btn = document.getElementById('btn-ota-upload');
      const progressContainer = document.getElementById('ota-progress-container');
      const progressBar = document.getElementById('ota-progress-bar');
      const progressPercent = document.getElementById('ota-progress-percent');
      const progressText = document.getElementById('ota-progress-text');

      if (!fileInput.files || fileInput.files.length === 0) {
        showOtaStatus('⚠️ Por favor selecciona un archivo .bin de firmware antes de continuar.', 'danger');
        return;
      }

      const file = fileInput.files[0];
      if (!file.name.endsWith('.bin')) {
        showOtaStatus('⚠️ El archivo seleccionado debe tener extensión .bin.', 'danger');
        return;
      }

      const formData = new FormData();
      formData.append('update', file);

      btn.disabled = true;
      progressContainer.style.display = 'block';
      progressBar.style.width = '0%';
      progressPercent.innerText = '0%';
      progressText.innerText = 'Enviando ' + file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)...';
      showOtaStatus('⏳ Flasheando partición secundaria... Por favor NO desconectes la alimentación del nodo.', 'info');

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressBar.style.width = percent + '%';
          progressPercent.innerText = percent + '%';
        }
      };

      xhr.onload = () => {
        if (xhr.status === 200) {
          progressBar.style.width = '100%';
          progressPercent.innerText = '100%';
          showOtaStatus('✅ ¡Actualización de firmware exitosa! El Gateway se está reiniciando en la nueva partición. La página se recargará en 12 segundos...', 'success');
          setTimeout(() => { window.location.href = '/'; }, 12000);
        } else {
          showOtaStatus('❌ Error al flashear firmware (HTTP ' + xhr.status + '). Revisa la consola serie.', 'danger');
          btn.disabled = false;
        }
      };

      xhr.onerror = () => {
        showOtaStatus('❌ Error de conexión durante la transmisión del firmware.', 'danger');
        btn.disabled = false;
      };

      xhr.send(formData);
    }

    if (window.location.hash === '#ota' || window.location.pathname === '/update') {
      toggleOta();
    }

    setInterval(fetchSensors, 3000);
    connectWS();
    fetchSensors();
  </script>
</body>
</html>
)rawliteral";

#endif // GATEWAY_HTML_H
