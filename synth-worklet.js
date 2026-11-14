// The bundled reference module runs on the audio thread, using its own sample clock.
class AMSynthProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.engine = new WebAssembly.Instance(options.processorOptions.module, {}).exports;
    this.engine._initialize?.();
    this.engine.gs_init(sampleRate);
    options.processorOptions.parameters.forEach((value, address) => this.engine.gs_parameter(address, value));
    this.samples = new Float32Array(this.engine.memory.buffer);
    this.frames = 0;
    this.failed = false;
    this.port.onmessage = ({data}) => {
      if (this.failed) return;
      try {
        if (data.type === 'note') this.engine.gs_note(data.note, data.velocity);
        if (data.type === 'parameter') this.engine.gs_parameter(data.address, data.value);
        if (data.type === 'panic') this.engine.gs_panic();
      } catch (error) { this.fail(error); }
    };
  }
  fail(error) {
    this.failed = true;
    this.port.postMessage({type: 'fault', message: String(error)});
  }
  process(inputs, outputs) {
    const output = outputs[0][0];
    if (this.failed) { output.fill(0); return true; }
    try {
      const offset = this.engine.gs_render(output.length) >>> 2;
      for (let i = 0; i < output.length; i++) {
        const sample = this.samples[offset + i];
        output[i] = Number.isFinite(sample) ? Math.max(-1, Math.min(1, sample)) : 0;
      }
      this.frames += output.length;
      if (this.frames >= 2048) {
        this.frames = 0;
        this.port.postMessage({type: 'status', voices: this.engine.gs_active_voices(), lfo: this.engine.gs_lfo_status() >>> 0});
      }
    } catch (error) { output.fill(0); this.fail(error); }
    return true;
  }
}
registerProcessor('amsynth', AMSynthProcessor);
