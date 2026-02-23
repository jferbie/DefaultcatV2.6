/* app.js — DefaultCab v2.6 (dark) BLE code + single-screen layout */

(function DefaultCabV2() {
  'use strict';

  /* BLE UUIDs (original) */
  const SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214";
  const WRITE_UUID   = "19b10002-e8f2-537e-4f6c-d104768a1214";
  const NOTIFY_UUID  = "19b10001-e8f2-537e-4f6c-d104768a1214";

  let device = null;
  let server = null;
  let service = null;
  let writeChar = null;
  let notifyChar = null;
  let isConnected = false;

  let lastWriteTs = 0;
  const WRITE_MIN_INTERVAL_MS = 120;

  const $id = id => document.getElementById(id);

  /* Cached elements */
  const runTab = $id('runTab');
  const runScreen = $id('runScreen');

  const throttle = $id('throttle');
  const throttleVal = $id('throttleVal');
  const throttleOutput = $id('throttleOutput');
  const analog = $id('analog');
  const speedEl = $id('speed');

  const forwardBtn = $id('forwardBtn');
  const reverseBtn = $id('reverseBtn');
  const stopBtn = $id('stopBtn');

  const a1Btn = $id('a1Btn');
  const a2Btn = $id('a2Btn');

  const saveSettingsBtn = $id('saveSettings');
  const boostPower = $id('boostPower');
  const boostDuration = $id('boostDuration');
  const dirDelay = $id('dirDelay');
  const locoNameInput = $id('locoNameInput');
  const locoNameEl = $id('locoName');

  /* UI: tabs */
  function showRun() {
    if (runScreen) runScreen.style.display = 'grid';
    if (runTab) runTab.classList.add('active');
  }
  if (runTab) runTab.addEventListener('click', showRun);

  /* Connection UI */
  function updateConnectionUI(connected) {
    const status = $id('status');
    const connectBtn = $id('connectBtn');
    const deviceNameEl = $id('deviceName');
    if (!status || !connectBtn) return;
    if (connected) {
      status.textContent = 'Connected';
      connectBtn.textContent = 'Disconnect';
      if (device && deviceNameEl) deviceNameEl.textContent = device.name || 'Device';
      isConnected = true;
    } else {
      status.textContent = 'Disconnected';
      connectBtn.textContent = 'Connect';
      if (deviceNameEl) deviceNameEl.textContent = '—';
      isConnected = false;
    }
  }

  /* BLE functions */
  async function connectBLE() {
    if (!navigator.bluetooth) {
      console.warn('Web Bluetooth not available');
      return;
    }
    try {
      device = await navigator.bluetooth.requestDevice({ filters: [{ services: [SERVICE_UUID] }] });
      if (!device) return;
      device.addEventListener && device.addEventListener('gattserverdisconnected', onDisconnected);
      server = await device.gatt.connect();
      service = await server.getPrimaryService(SERVICE_UUID);
      writeChar = await service.getCharacteristic(WRITE_UUID);
      notifyChar = await service.getCharacteristic(NOTIFY_UUID);
      if (notifyChar && notifyChar.startNotifications) {
        await notifyChar.startNotifications();
        notifyChar.addEventListener('characteristicvaluechanged', handleNotify);
      }
      updateConnectionUI(true);
    } catch (err) {
      console.error('BLE connect error', err);
      onDisconnected();
    }
  }

  function onDisconnected() {
    isConnected = false;
    server = null; service = null; writeChar = null; notifyChar = null;
    updateConnectionUI(false);
  }

  function disconnectBLE() {
    try {
      if (device && device.gatt && device.gatt.connected) device.gatt.disconnect();
    } catch (err) {
      console.error('BLE disconnect error', err);
    } finally {
      onDisconnected();
    }
  }

  function handleNotify(event) {
    try {
      const raw = event.target && event.target.value ? event.target.value : event.target;
      const text = new TextDecoder().decode(raw);
      const parts = String(text).split(',');
      const speed = parts[1] ? (parts[1].split(':')[1] || '') : '';
      if (speedEl) speedEl.textContent = speed;
      console.log('Notify:', text.trim());
    } catch (err) {
      console.error('handleNotify error:', err);
    }
  }

  async function sendCommand(cmd) {
    try {
      if (!writeChar || !isConnected) {
        console.log('[sendCommand - no BLE]', cmd);
        return;
      }
      const now = Date.now();
      if (now - lastWriteTs < WRITE_MIN_INTERVAL_MS) return;
      lastWriteTs = now;
      await writeChar.writeValue(new TextEncoder().encode(String(cmd)));
    } catch (err) {
      console.error('sendCommand error', err);
    }
  }

  /* Wire connect button */
  (function wireConnectBtn() {
    const btn = $id('connectBtn');
    if (!btn) return;
    btn.addEventListener('click', () => {
      if (!isConnected) connectBLE();
      else disconnectBLE();
    });
  })();

  /* Throttle dial module (smoothing + canvas) — adjusted for new canvas size */
  (function ThrottleDialModule(){
    const canvas = document.getElementById('throttleDial');
    const percentEl = document.getElementById('throttlePercent');
    const throttleInput = document.getElementById('throttle');
    const throttleValEl = document.getElementById('throttleVal');
    const throttleOutEl = document.getElementById('throttleOutput');

    if (!canvas || !throttleInput) return;

    const deadband = 1;
    const rampMs = 120;
    const sendIntervalMs = 80;

    const ctx = canvas.getContext('2d');
    const DPR = window.devicePixelRatio || 1;
    const W = canvas.width;   // 260
    const H = canvas.height;  // 130
    canvas.style.width = W + 'px';
    canvas.style.height = H + 'px';
    canvas.width = W * DPR;
    canvas.height = H * DPR;
    ctx.scale(DPR, DPR);

    const cx = (W / DPR) / 2;
    const cy = (H / DPR);
    const radius = Math.min(cx, cy) - 8;

    function drawDialStatic() {
      ctx.clearRect(0, 0, W, H);
      const isDark = true;
      const arcBg = isDark ? 'rgba(255,255,255,0.06)' : 'rgba(0,0,0,0.06)';
      const tickColor = isDark ? 'rgba(255,255,255,0.12)' : 'rgba(0,0,0,0.12)';

      ctx.beginPath();
      ctx.lineWidth = 10;
      ctx.strokeStyle = arcBg;
      ctx.arc(cx, cy, radius, Math.PI, 0, false);
      ctx.stroke();

      const ticks = 10;
      for (let i = 0; i <= ticks; i++) {
        const t = i / ticks;
        const angle = Math.PI + t * Math.PI;
        const inner = radius - 8;
        const outer = radius + 2;
        const ix = cx + Math.cos(angle) * inner;
        const iy = cy + Math.sin(angle) * inner;
        const ox = cx + Math.cos(angle) * outer;
        const oy = cy + Math.sin(angle) * outer;
        ctx.beginPath();
        ctx.lineWidth = (i % 5 === 0) ? 2.2 : 1.2;
        ctx.strokeStyle = tickColor;
        ctx.moveTo(ix, iy);
        ctx.lineTo(ox, oy);
        ctx.stroke();
      }
    }

    function drawNeedle(norm) {
      drawDialStatic();
      const isDark = true;
      const needleColor = isDark ? '#e6eef8' : '#222';
      const angle = Math.PI + norm * Math.PI;
      const nx = cx + Math.cos(angle) * (radius - 18);
      const ny = cy + Math.sin(angle) * (radius - 18);

      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(nx, ny);
      ctx.lineWidth = 4;
      ctx.strokeStyle = needleColor;
      ctx.lineCap = 'round';
      ctx.stroke();

      ctx.beginPath();
      ctx.fillStyle = needleColor;
      ctx.arc(cx, cy, 6, 0, Math.PI * 2);
      ctx.fill();
    }

    let current = Number(throttleInput.value || 0);
    let target = current;
    let lastSendTs = 0;
    let animFrame = null;

    function setTarget(v) {
      target = Math.max(0, Math.min(100, Math.round(v)));
      if (throttleValEl) throttleValEl.textContent = target + '%';
      if (percentEl) percentEl.textContent = target + '%';
      if (throttleOutEl) throttleOutEl.textContent = target + '%';
      if (!animFrame) animFrame = requestAnimationFrame(step);
    }

    function step() {
      const diff = target - current;
      if (Math.abs(diff) <= 0.01) current = target;
      else {
        const alpha = Math.min(1, 16 / Math.max(16, rampMs));
        current += diff * alpha;
      }

      const norm = current / 100;
      drawNeedle(norm);

      const now = Date.now();
      // When close to target, prefer sending the target; otherwise send current for smooth ramp
      if (Math.abs(Math.round(current) - Math.round(target)) <= deadband) {
        if (now - lastSendTs > sendIntervalMs && Math.round(current) !== Math.round(target)) {
          lastSendTs = now;
          if (typeof sendCommand === 'function') sendCommand('T' + Math.round(target));
        }
      } else {
        if (now - lastSendTs > sendIntervalMs) {
          lastSendTs = now;
          if (typeof sendCommand === 'function') sendCommand('T' + Math.round(current));
        }
      }

      if (Math.abs(current - target) > 0.01) animFrame = requestAnimationFrame(step);
      else animFrame = null;
    }

    drawDialStatic();
    drawNeedle(current / 100);

    throttleInput.addEventListener('input', function () {
      const v = Number(this.value || 0);
      setTarget(v);
    });

    let pointerActive = false;
    function dialPointerToValue(clientX, clientY) {
      const rect = canvas.getBoundingClientRect();
      const x = clientX - rect.left;
      const y = clientY - rect.top;
      const dx = x - cx;
      const dy = y - cy;
      const angle = Math.atan2(dy, dx);
      let t = (angle - Math.PI) / Math.PI;
      t = Math.max(0, Math.min(1, t));
      return Math.round(t * 100);
    }

    canvas.addEventListener('pointerdown', (e) => {
      pointerActive = true;
      canvas.setPointerCapture && canvas.setPointerCapture(e.pointerId);
      const v = dialPointerToValue(e.clientX, e.clientY);
      throttleInput.value = v;
      setTarget(v);
    });
    canvas.addEventListener('pointermove', (e) => {
      if (!pointerActive) return;
      const v = dialPointerToValue(e.clientX, e.clientY);
      throttleInput.value = v;
      setTarget(v);
    });
    canvas.addEventListener('pointerup', (e) => {
      pointerActive = false;
      canvas.releasePointerCapture && canvas.releasePointerCapture(e.pointerId);
    });
    canvas.addEventListener('pointercancel', () => { pointerActive = false; });

    // Expose draw function for external callers (keeps compatibility)
    window.drawThrottleDial = function(v) {
      drawNeedle(Math.max(0, Math.min(100, Number(v))) / 100);
    };

  })();

  /* Prevent page scroll while dragging the throttle slider */
  (function lockScrollDuringDrag(){
    const slider = document.getElementById('throttle');
    if (!slider) return;
    let dragging = false;

    slider.addEventListener('pointerdown', (e) => {
      dragging = true;
      document.body.style.overflow = 'hidden';
      if (slider.setPointerCapture) slider.setPointerCapture(e.pointerId);
    });

    function endDrag(e) {
      dragging = false;
      document.body.style.overflow = '';
      if (e && slider.releasePointerCapture) slider.releasePointerCapture(e.pointerId);
    }

    slider.addEventListener('pointerup', endDrag);
    slider.addEventListener('pointercancel', endDrag);
    window.addEventListener('blur', () => { if (dragging) { dragging = false; document.body.style.overflow = ''; } });
  })();

  /* Accessories: ensure in right column and bind */
  (function ensureAccessoriesInRightCol(){
    const right = $id('rightCol');
    const acc = document.querySelector('.accessory-row');
    if (!right || !acc) return;
    // Place accessory-row inside the Accessories card frame (first .card-frame)
    const accCard = right.querySelector('.card-frame');
    if (accCard && acc.parentElement !== accCard) accCard.appendChild(acc);

    function bind(btn, token){
      if (!btn) return;
      btn.classList.remove('on'); btn.classList.add('off');
      btn.onclick = function(){
        const isOn = btn.classList.toggle('on');
        btn.classList.toggle('off', !isOn);
        sendCommand(`${token} ${isOn ? 1 : 0}`);
      };
    }
    bind($id('a1Btn'), 'A1'); bind($id('a2Btn'), 'A2');
  })();

  /* Direction / Stop */
  function setDirActive(side){
    if (!forwardBtn || !reverseBtn) return;
    if (side === 'forward'){
      forwardBtn.classList.add('active');
      reverseBtn.classList.remove('active');
    } else if (side === 'reverse'){
      reverseBtn.classList.add('active');
      forwardBtn.classList.remove('active');
    }
  }

  if (forwardBtn) forwardBtn.addEventListener('click', () => { setDirActive('forward'); sendCommand('D1'); });
  if (reverseBtn) reverseBtn.addEventListener('click', () => { setDirActive('reverse'); sendCommand('D0'); });
  if (stopBtn) stopBtn.addEventListener('click', () => {
    if (throttle) throttle.value = 0;
    if (throttleVal) throttleVal.textContent = '0%';
    if (throttleOutput) throttleOutput.textContent = '0%';
    if (analog) analog.textContent = '0%';
    sendCommand('T0');
  });

  /* Settings Modal */
  (function setupSettingsModal() {
    const modal = $id('settingsModal');
    const openBtn = $id('openSettingsBtn');
    const closeBtn = $id('closeSettingsBtn');
    const modalCloseBtn = $id('modalCloseBtn');

    if (!modal || !openBtn) {
      return;
    }

    function openModal() {
      modal.classList.add('show');
    }

    function closeModal() {
      modal.classList.remove('show');
    }

    if (openBtn) openBtn.addEventListener('click', openModal);
    if (closeBtn) closeBtn.addEventListener('click', closeModal);
    if (modalCloseBtn) modalCloseBtn.addEventListener('click', closeModal);

    // Close modal when clicking outside of it
    if (modal) {
      modal.addEventListener('click', (e) => {
        if (e.target === modal) closeModal();
      });
    }
  })();

  /* Settings value displays */
  if (boostPower) {
    const boostPowerVal = $id('boostPowerVal');
    boostPower.addEventListener('input', () => {
      if (boostPowerVal) boostPowerVal.textContent = boostPower.value;
    });
  }
  if (boostDuration) {
    const boostDurationVal = $id('boostDurationVal');
    boostDuration.addEventListener('input', () => {
      if (boostDurationVal) boostDurationVal.textContent = boostDuration.value;
    });
  }
  if (dirDelay) {
    const dirDelayVal = $id('dirDelayVal');
    dirDelay.addEventListener('input', () => {
      if (dirDelayVal) dirDelayVal.textContent = dirDelay.value;
    });
  }

  /* Settings save */
  if (saveSettingsBtn) {
    saveSettingsBtn.addEventListener('click', () => {
      const sanitizedLocoName = String(locoNameInput ? (locoNameInput.value || '') : '')
        .replace(/\s+/g, ' ')
        .trim()
        .slice(0, 20);
      const p = boostPower ? (boostPower.value || 0) : 0;
      const d = boostDuration ? (boostDuration.value || 0) : 0;
      const delay = dirDelay ? (dirDelay.value || 0) : 0;

      if (locoNameInput) locoNameInput.value = sanitizedLocoName || 'Default';
      if (locoNameEl) locoNameEl.textContent = `Loco: ${sanitizedLocoName || 'Default'}`;

      sendCommand(`BOOSTCFG ${p} ${d}`);
      setTimeout(() => sendCommand(`DELAYCFG ${delay}`), WRITE_MIN_INTERVAL_MS + 20);
      if (sanitizedLocoName) {
        setTimeout(() => sendCommand(`NAME ${sanitizedLocoName}`), (WRITE_MIN_INTERVAL_MS * 2) + 40);
      }
      saveSettingsBtn.disabled = true;
      const prev = saveSettingsBtn.textContent;
      saveSettingsBtn.textContent = 'Saved';
      setTimeout(() => { saveSettingsBtn.disabled = false; saveSettingsBtn.textContent = prev; }, 900);
      // Close modal after saving
      const modal = $id('settingsModal');
      if (modal) setTimeout(() => modal.classList.remove('show'), 500);
    });
  }

  /* Ensure rotated slider track length matches throttle-slider-wrap height */
  (function bindThrottleSizing() {
    const slider = document.getElementById('throttle');
    const wrap = document.querySelector('.throttle-slider-wrap');

    if (!slider || !wrap) return;

    function sizeSlider() {
      const wrapRect = wrap.getBoundingClientRect();
      // target visible vertical length: 80% of wrap height minus padding for labels/thumb
      const targetLength = Math.max(160, Math.floor(wrapRect.height * 0.8 - 24));
      slider.style.width = targetLength + 'px';
      slider.style.height = '18px';
    }

    // Initial sizing
    sizeSlider();

    if (typeof ResizeObserver !== 'undefined') {
      const ro = new ResizeObserver(sizeSlider);
      ro.observe(wrap);
      ro.observe(document.body);
    } else {
      window.addEventListener('resize', sizeSlider);
    }
  })();

  /* Expose debug helpers */
  window.DefaultCab = { connectBLE, disconnectBLE, sendCommand, showRun };

  /* Init */
  (function init() {
    if (locoNameInput && locoNameEl) {
      const initialName = String(locoNameEl.textContent || '').replace(/^\s*Loco:\s*/i, '').trim() || 'Default';
      locoNameInput.value = initialName;
      locoNameEl.textContent = `Loco: ${initialName}`;
    }
    showRun();
    if ($id('a1Btn') && !$id('a1Btn').classList.contains('on')) $id('a1Btn').classList.add('off');
    if ($id('a2Btn') && !$id('a2Btn').classList.contains('on')) $id('a2Btn').classList.add('off');
    updateConnectionUI(false);
    // default direction visual state: FOR selected
    try { setDirActive && setDirActive('forward'); } catch(e){}
  })();

})();
