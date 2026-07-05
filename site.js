/* ASTRA-TUNE site: hero pitch instrument + live browser demo. */
(() => {
  'use strict';

  const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const SCALES = {
    major:     [1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1],
    minor:     [1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0],
    majpent:   [1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0],
    minpent:   [1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0],
    chromatic: [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
  };
  const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const $ = (id) => document.getElementById(id);

  /* ---------------- hero instrument ---------------- */
  const canvas = $('pitchCanvas');
  if (canvas) {
    const ctx2d = canvas.getContext('2d');
    const hint = $('instrumentHint');
    let W = 0, H = 0, dpr = 1;
    let detune = reducedMotion ? 0 : 1.6; // semitones away from the target line
    let vel = 0;
    let dragging = false;
    let lastY = 0;
    let t = 0;
    let interacted = false;

    const resize = () => {
      dpr = Math.min(2, window.devicePixelRatio || 1);
      W = canvas.clientWidth;
      H = canvas.clientHeight;
      canvas.width = Math.round(W * dpr);
      canvas.height = Math.round(H * dpr);
      ctx2d.setTransform(dpr, 0, 0, dpr, 0, 0);
    };
    resize();
    window.addEventListener('resize', resize);

    const semiPx = () => H / 7; // 7 semitone rows visible
    const midY = () => H * 0.5;

    canvas.addEventListener('pointerdown', (e) => {
      dragging = true;
      interacted = true;
      lastY = e.clientY;
      canvas.classList.add('dragging');
      canvas.setPointerCapture(e.pointerId);
      if (hint) hint.style.opacity = '0';
    });
    canvas.addEventListener('pointermove', (e) => {
      if (!dragging) return;
      detune += (e.clientY - lastY) / semiPx();
      detune = Math.max(-3.2, Math.min(3.2, detune));
      lastY = e.clientY;
      vel = 0;
    });
    const release = () => {
      dragging = false;
      canvas.classList.remove('dragging');
    };
    canvas.addEventListener('pointerup', release);
    canvas.addEventListener('pointercancel', release);

    const draw = () => {
      t += 1 / 60;
      // spring toward the target note line when not dragging
      if (!dragging) {
        const k = 90, damp = 9;
        const acc = -k * detune - damp * vel;
        vel += acc / 60;
        detune += vel / 60;
        if (!interacted && !reducedMotion) {
          // idle demo: drift off pitch occasionally so the snap is visible
          if (Math.abs(detune) < 0.02 && Math.abs(vel) < 0.02 && Math.random() < 0.004) {
            detune = (Math.random() < 0.5 ? -1 : 1) * (1 + Math.random() * 1.6);
            vel = 0;
          }
        }
      }

      ctx2d.clearRect(0, 0, W, H);

      // semitone grid
      const rows = 7;
      ctx2d.font = '11px "IBM Plex Mono", monospace';
      for (let r = 0; r < rows; r++) {
        const y = (r + 0.5) * (H / rows);
        const center = r === Math.floor(rows / 2);
        ctx2d.strokeStyle = center ? 'rgba(53,224,255,0.35)' : 'rgba(92,111,133,0.18)';
        ctx2d.lineWidth = 1;
        ctx2d.beginPath();
        ctx2d.moveTo(0, y);
        ctx2d.lineTo(W, y);
        ctx2d.stroke();
        if (r < rows - 1) { // last row's label would collide with the readout line
          ctx2d.fillStyle = center ? 'rgba(53,224,255,0.8)' : 'rgba(92,111,133,0.6)';
          const rowMidi = 64 + (3 - r); // center row = E4, one semitone per row
          ctx2d.fillText(NOTE_NAMES[((rowMidi % 12) + 12) % 12] + (Math.floor(rowMidi / 12) - 1), 10, y - 6);
        }
      }

      // input trace (muted): where the singer actually is
      const drawTrace = (offsetSemis, color, width, glow) => {
        ctx2d.strokeStyle = color;
        ctx2d.lineWidth = width;
        ctx2d.shadowColor = glow || 'transparent';
        ctx2d.shadowBlur = glow ? 14 : 0;
        ctx2d.beginPath();
        for (let x = 0; x <= W; x += 3) {
          const p = x / W;
          const wobble = reducedMotion ? 0
            : Math.sin(p * 26 + t * 5.5) * 2.2 + Math.sin(p * 7 - t * 1.8) * 3.4;
          const y = midY() + offsetSemis * semiPx() * (0.25 + 0.75 * p) + wobble * (0.3 + 0.7 * p) * 0.4;
          if (x === 0) ctx2d.moveTo(x, y); else ctx2d.lineTo(x, y);
        }
        ctx2d.stroke();
        ctx2d.shadowBlur = 0;
      };
      drawTrace(detune, 'rgba(92,111,133,0.55)', 1.5, null);
      drawTrace(detune * 0.12, '#35e0ff', 2.2, 'rgba(53,224,255,0.8)');

      // readout
      const cents = Math.round(detune * 100);
      ctx2d.fillStyle = 'rgba(200,214,229,0.9)';
      ctx2d.font = '12px "IBM Plex Mono", monospace';
      ctx2d.fillText('input ' + (cents >= 0 ? '+' : '') + cents + ' cents', 10, H - 14);
      ctx2d.fillStyle = '#35e0ff';
      ctx2d.fillText('output E4 locked', 150, H - 14);

      if (!reducedMotion) requestAnimationFrame(draw);
    };
    draw();
    if (reducedMotion) { detune = 0; draw(); }
  }

  /* ---------------- mini piano (features) ---------------- */
  const miniPiano = $('miniPiano');
  if (miniPiano) {
    const mask = SCALES.major;
    NOTE_NAMES.forEach((n, i) => {
      const el = document.createElement('div');
      el.className = 'pk' + (n.includes('#') ? ' black' : '') + (mask[i] ? ' in-scale' : '');
      el.textContent = n;
      miniPiano.appendChild(el);
    });
  }

  /* ---------------- live demo ---------------- */
  let audio = null, node = null, srcNode = null, micStream = null, demoVoice = null, latest = null;

  NOTE_NAMES.forEach((n, i) => {
    const opt = document.createElement('option');
    opt.value = String(i);
    opt.textContent = n;
    $('demoKey').appendChild(opt);
  });

  const sendParams = () => {
    if (!node) return;
    node.port.postMessage({
      retuneMs: +$('demoRetune').value,
      mix: +$('demoMix').value / 100,
      humanize: 0,
      bypass: false,
      keyRoot: +$('demoKey').value,
      scaleMask: SCALES[$('demoScale').value],
    });
  };
  const updateLabels = () => {
    const r = +$('demoRetune').value;
    $('demoRetuneVal').textContent = r === 0 ? 'hard' : r + ' ms';
    $('demoMixVal').textContent = $('demoMix').value + '%';
  };
  ['demoKey', 'demoScale', 'demoRetune', 'demoMix'].forEach((id) =>
    $(id).addEventListener('input', () => { updateLabels(); sendParams(); }));

  const ensureAudio = async () => {
    if (!audio) {
      audio = new AudioContext();
      await audio.audioWorklet.addModule('tuner-worklet.js');
      node = new AudioWorkletNode(audio, 'tuner', { outputChannelCount: [1] });
      const comp = audio.createDynamicsCompressor();
      node.connect(comp).connect(audio.destination);
      node.port.onmessage = (e) => { latest = e.data; window.__tunerDebug = e.data; };
      sendParams();
    }
    await audio.resume();
  };

  const stopSources = () => {
    if (srcNode) { try { srcNode.disconnect(); } catch (e) {} srcNode = null; }
    if (micStream) { micStream.getTracks().forEach((tr) => tr.stop()); micStream = null; }
    if (demoVoice) {
      clearInterval(demoVoice.timer);
      try { demoVoice.osc.stop(); demoVoice.vib.stop(); } catch (e) {}
      demoVoice = null;
    }
  };
  const setRunning = (running, label) => {
    $('stopBtn').disabled = !running;
    $('demoVoiceBtn').disabled = running;
    $('micBtn').disabled = running;
    $('demoFreq').textContent = label;
  };

  $('demoVoiceBtn').addEventListener('click', async () => {
    await ensureAudio();
    stopSources();
    const osc = audio.createOscillator();
    osc.type = 'sawtooth';
    const vib = audio.createOscillator();
    vib.frequency.value = 5.5;
    const vibGain = audio.createGain();
    vibGain.gain.value = 18;
    vib.connect(vibGain).connect(osc.detune);
    const lp = audio.createBiquadFilter();
    lp.type = 'lowpass'; lp.frequency.value = 1400; lp.Q.value = 0.7;
    const formant = audio.createBiquadFilter();
    formant.type = 'peaking'; formant.frequency.value = 700; formant.gain.value = 6;
    const gain = audio.createGain();
    gain.gain.value = 0.35;
    osc.connect(lp).connect(formant).connect(gain).connect(node);
    const melody = [57, 59, 60, 62, 64, 65, 67, 64, 62, 60];
    let step = 0;
    const stepMelody = () => {
      const m = melody[step % melody.length];
      step += 1;
      const cents = (Math.random() < 0.5 ? -1 : 1) * (30 + Math.random() * 45);
      const f = 440 * Math.pow(2, (m - 69) / 12) * Math.pow(2, cents / 1200);
      osc.frequency.setTargetAtTime(f, audio.currentTime, 0.04);
    };
    stepMelody();
    osc.start();
    vib.start();
    demoVoice = { osc, vib, timer: setInterval(stepMelody, 550) };
    srcNode = gain;
    setRunning(true, 'demo singer running');
  });

  $('micBtn').addEventListener('click', async () => {
    try {
      await ensureAudio();
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
      });
      stopSources();
      micStream = stream;
      srcNode = audio.createMediaStreamSource(stream);
      srcNode.connect(node);
      setRunning(true, 'live mic running');
    } catch (err) {
      $('demoFreq').textContent = 'mic unavailable (' + err.name + ')';
    }
  });

  $('stopBtn').addEventListener('click', () => {
    stopSources();
    setRunning(false, 'idle');
    $('demoNote').textContent = '--';
    $('demoNeedle').style.left = '50%';
    latest = null;
  });

  const tick = () => {
    requestAnimationFrame(tick);
    if (latest && latest.freq > 0 && latest.targetMidi !== null && latest.targetMidi >= 0) {
      const tm = latest.targetMidi;
      $('demoNote').textContent = NOTE_NAMES[((tm % 12) + 12) % 12] + (Math.floor(tm / 12) - 1);
      $('demoFreq').textContent = latest.freq.toFixed(1) + ' Hz';
      const cents = Math.max(-50, Math.min(50, (latest.midi - tm) * 100));
      $('demoNeedle').style.left = (50 + cents) + '%';
    }
  };
  updateLabels();
  tick();
})();
