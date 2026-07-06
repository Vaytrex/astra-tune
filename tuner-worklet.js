// ASTRA-TUNE audio engine.
// Runs on the audio thread: YIN pitch detection on a 2x-decimated signal,
// scale-aware target selection, and a dual-tap delay-line pitch shifter.

class TunerProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    const fs = sampleRate;
    this.fs = fs;

    // --- pitch shifter (dual crossfading delay taps over a ring buffer) ---
    this.rbBits = 15;
    this.rbSize = 1 << this.rbBits;
    this.rbMask = this.rbSize - 1;
    this.rb = new Float32Array(this.rbSize);
    this.w = 0;
    this.win = Math.round(0.03 * fs); // 30 ms grain
    this.phase = 0;

    // --- pitch detection (YIN on 2x-decimated audio) ---
    this.decFs = fs / 2;
    this.decSize = 1024; // ~42 ms window at 24 kHz
    this.decBuf = new Float32Array(this.decSize);
    this.decPos = 0;
    this.decHold = 0;
    this.decToggle = false;
    this.lin = new Float32Array(this.decSize);
    this.maxLag = Math.min(Math.floor(this.decFs / 60), (this.decSize >> 1) - 1);
    this.minLag = Math.max(2, Math.floor(this.decFs / 1000));
    this.d = new Float32Array(this.maxLag + 1);
    this.c = new Float32Array(this.maxLag + 1);
    this.hop = 512; // full-rate samples between detections (~10.7 ms)
    this.sinceHop = 0;

    // --- correction state / parameters (set via port messages) ---
    this.shift = 0;       // current shift in semitones
    this.targetShift = 0;
    this.retuneMs = 20;
    this.mix = 1;
    this.humanize = 0;    // 0..1, scales back the correction amount
    this.bypass = false;
    this.keyRoot = 0;
    this.scaleMask = [1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1];

    this.freq = 0;
    this.clarity = 0;
    this.midi = null;
    this.targetMidi = null;
    this.msgCounter = 0;

    this.port.onmessage = (e) => Object.assign(this, e.data);
  }

  readRb(delay) {
    const pos = this.w - delay;
    const i0 = Math.floor(pos);
    const frac = pos - i0;
    const s0 = this.rb[i0 & this.rbMask];
    const s1 = this.rb[(i0 + 1) & this.rbMask];
    return s0 + (s1 - s0) * frac;
  }

  runDetect() {
    const N = this.decSize;
    const lin = this.lin;
    for (let i = 0; i < N; i++) lin[i] = this.decBuf[(this.decPos + i) % N];

    let rms = 0;
    for (let i = 0; i < N; i++) rms += lin[i] * lin[i];
    rms = Math.sqrt(rms / N);
    this.level = rms;
    if (rms < 0.004) { this.freq = 0; this.clarity = 0; return; }

    const half = N >> 1;
    const maxLag = this.maxLag;
    const d = this.d, c = this.c;
    for (let lag = 1; lag <= maxLag; lag++) {
      let s = 0;
      for (let i = 0; i < half; i++) {
        const df = lin[i] - lin[i + lag];
        s += df * df;
      }
      d[lag] = s;
    }
    // cumulative-mean-normalized difference
    c[0] = 1;
    let run = 0;
    for (let lag = 1; lag <= maxLag; lag++) {
      run += d[lag];
      c[lag] = d[lag] * lag / (run || 1e-12);
    }
    let tau = -1;
    for (let lag = this.minLag; lag <= maxLag; lag++) {
      if (c[lag] < 0.15) {
        while (lag + 1 <= maxLag && c[lag + 1] < c[lag]) lag++;
        tau = lag;
        break;
      }
    }
    if (tau < 0) { this.freq = 0; this.clarity = 0; return; }

    let t = tau;
    if (tau > 1 && tau < maxLag) {
      const a = c[tau - 1], b = c[tau], g = c[tau + 1];
      const denom = a - 2 * b + g;
      if (Math.abs(denom) > 1e-12) t = tau + 0.5 * (a - g) / denom;
    }
    this.freq = this.decFs / t;
    this.clarity = Math.max(0, Math.min(1, 1 - c[tau]));
  }

  updateTarget() {
    if (this.freq > 0 && this.clarity > 0.5) {
      const midi = 69 + 12 * Math.log2(this.freq / 440);
      const center = Math.round(midi);
      let best = null, bestDist = Infinity;
      for (let n = center - 7; n <= center + 7; n++) {
        const pc = ((n - this.keyRoot) % 12 + 12) % 12;
        if (!this.scaleMask[pc]) continue;
        const dist = Math.abs(midi - n);
        if (dist < bestDist) { bestDist = dist; best = n; }
      }
      this.midi = midi;
      this.targetMidi = best;
      this.targetShift = best === null ? 0 : (best - midi) * (1 - this.humanize);
    } else {
      this.midi = null;
      this.targetMidi = null;
      this.targetShift = 0;
    }
    if (++this.msgCounter >= 4) {
      this.msgCounter = 0;
      this.port.postMessage({
        freq: this.freq,
        clarity: this.clarity,
        midi: this.midi,
        targetMidi: this.targetMidi,
        shift: this.shift,
        level: this.level || 0,
      });
    }
  }

  process(inputs, outputs) {
    const input = inputs[0];
    const output = outputs[0];
    if (!output || output.length === 0) return true;
    const out = output[0];
    const inCh = input && input.length ? input[0] : null;
    const n = out.length;

    const retuneSec = Math.max(this.retuneMs, 0.5) / 1000;
    const alpha = 1 - Math.exp(-1 / (this.fs * retuneSec));
    const win = this.win;
    const LN2_12 = Math.LN2 / 12;

    for (let i = 0; i < n; i++) {
      const x = inCh ? inCh[i] : 0;

      // feed the decimated detection buffer (2-sample average = cheap AA)
      if (this.decToggle) {
        this.decBuf[this.decPos] = (x + this.decHold) * 0.5;
        this.decPos = (this.decPos + 1) % this.decSize;
      } else {
        this.decHold = x;
      }
      this.decToggle = !this.decToggle;
      if (++this.sinceHop >= this.hop) {
        this.sinceHop = 0;
        this.runDetect();
        this.updateTarget();
      }

      this.shift += (this.targetShift - this.shift) * alpha;
      const ratio = Math.exp(this.shift * LN2_12);

      this.rb[this.w] = x;
      let phase = this.phase + (1 - ratio) / win;
      phase -= Math.floor(phase);
      this.phase = phase;
      const p2 = (phase + 0.5) % 1;
      const y = Math.sin(Math.PI * phase) * this.readRb(phase * win + 2) +
                Math.sin(Math.PI * p2) * this.readRb(p2 * win + 2);
      this.w = (this.w + 1) & this.rbMask;

      out[i] = this.bypass ? x : this.mix * y + (1 - this.mix) * x;
    }
    for (let ch = 1; ch < output.length; ch++) output[ch].set(out);
    return true;
  }
}

registerProcessor('tuner', TunerProcessor);
