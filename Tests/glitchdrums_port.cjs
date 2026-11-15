// SPDX-License-Identifier: GPL-3.0-only
const fs=require('node:fs'),assert=require('node:assert/strict');
(async()=>{
const bytes=fs.readFileSync('Instruments/GlitchDrums/glitchdrums.wasm'),module=await WebAssembly.compile(bytes);
assert.equal(WebAssembly.Module.imports(module).length,0);
let x=(await WebAssembly.instantiate(module,{})).exports;
x.gdp_boot();x.gdp_buttons(1<<12);x.gdp_tick(1);x.gdp_buttons(0);x.gdp_tick(2);
let native=fs.readFileSync('build/glitchdrums/stock.pcm'),ms=2,frames=0,peak=0,energy=0,worst=0,total=0;
for(let block=0;block<1723;block++){
 while(ms<Math.floor(frames*1000/44100))x.gdp_tick(++ms);
 let a=process.hrtime.bigint();x.gdp_audio(256);let us=Number(process.hrtime.bigint()-a)/1000;worst=Math.max(worst,us);total+=us;
 let left=new Float32Array(x.memory.buffer,x.gdp_left(),256),right=new Float32Array(x.memory.buffer,x.gdp_right(),256);
 for(let i=0;i<256;i++){for(let c=0;c<2;c++){let value=(c?right:left)[i];assert(Number.isFinite(value));peak=Math.max(peak,Math.abs(value));energy+=value*value;let actual=Math.trunc(Math.max(-1,Math.min(1,value))*32767);assert.equal(actual||0,native.readInt16LE(((frames+i)*2+c)*2),`PCM mismatch at ${frames+i}/${c}`);}}
 frames+=256;
}
assert(energy>1&&peak<=1.001);x.gdp_save();let length=x.gdp_state_size();assert(length>0&&length<=12276);
let state=new Uint8Array(x.memory.buffer,x.gdp_state(),length).slice();
let y=(await WebAssembly.instantiate(module,{})).exports;
assert.equal(y.gdp_restore(12277),0);new Uint8Array(y.memory.buffer,y.gdp_state(),length).set(state);assert.equal(y.gdp_restore(length),1);y.gdp_boot();assert(y.gdp_screen_generation()>0);
for(let button=2;button<=11;button++){y.gdp_buttons(1<<button);y.gdp_tick(++ms);y.gdp_buttons(0);y.gdp_tick(++ms);}
y.gdp_keys(1);y.gdp_tick(++ms);y.gdp_keys(0);y.gdp_tick(++ms);y.gdp_encoder(0,7);y.gdp_tick(++ms);y.gdp_audio(256);
console.log('PASS: full native/WASM stock PCM identical for 441,088 stereo frames; isolated instance, project restore, all screen buttons and key release');
console.log(`HOST ONLY WASM DSP mean ${(total/1723).toFixed(1)}us max ${worst.toFixed(1)}us; device unmeasured`);
})().catch(e=>{console.error(e);process.exitCode=1;});
