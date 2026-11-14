// Run: node Tests/web-runtime.cjs. Exercises the actual worklet against the bundled DSP.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
let Processor;
class Port { postMessage(message) { this.last = message; } }
class AudioWorkletProcessor { constructor() { this.port = new Port(); } }
const sandbox = { AudioWorkletProcessor, WebAssembly, Float32Array, Number, Math,
  sampleRate: 48000, registerProcessor(name, implementation) { Processor = implementation; } };
vm.runInNewContext(fs.readFileSync(path.join(root, 'synth-worklet.js'), 'utf8'), sandbox);
const parameters = [1,.015,.25,5000,.2,0,1,0,0,0,4,.5,1,0,120,3,1,1,1];
for (const id of ['pocket_analog', 'fm_bell']) {
  const module = new WebAssembly.Module(fs.readFileSync(path.join(root, 'Firmware/Modules', id + '.gsm5')));
  assert.equal(WebAssembly.Module.imports(module).length, 0);
  const manifest = JSON.parse(new TextDecoder().decode(WebAssembly.Module.customSections(module, 'gs5')[0]));
  assert.match(manifest.name, /^AMSynth/);
  const processor = new Processor({ processorOptions: { module, parameters } });
  const direct = new WebAssembly.Instance(module, {}).exports;
  direct._initialize?.(); direct.gs_init(48000);
  parameters.forEach((v, a) => direct.gs_parameter(a, v));
  const isolated = new Processor({ processorOptions: { module, parameters } });
  for (const note of [60,64,67,70]) {
    processor.port.onmessage({ data: { type:'note', note, velocity:100 } });
    direct.gs_note(note,100);
  }
  let energy = 0;
  for (let block = 0; block < 40; block++) {
    const audio = new Float32Array(128), silent = new Float32Array(128);
    assert.equal(processor.process([], [[audio]]), true);
    isolated.process([], [[silent]]); assert(silent.every(v => v === 0));
    const expected = new Float32Array(direct.memory.buffer, direct.gs_render(128), 128);
    for (let i=0;i<128;i++) {
      assert(Number.isFinite(audio[i]));
      assert.equal(audio[i], Math.max(-1, Math.min(1, expected[i])));
      energy += audio[i]*audio[i];
    }
  }
  assert(energy > .01); assert.equal(processor.port.last.type, 'status');
  processor.port.onmessage({ data: { type:'panic' } });
  const audio = new Float32Array(128); processor.process([], [[audio]]);
  assert(audio.every(v => Math.abs(v)<1e-6));
  console.log('PASS web runtime:', id, 'four voices, block parity, isolated instances, panic, telemetry');
}
