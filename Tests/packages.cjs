const fs=require('node:fs');
const assert=require('node:assert/strict');
const {webcrypto}=require('node:crypto');
globalThis.crypto=webcrypto;
(async()=>{
 const {readInstrumentPackage,compilePackageEngine,sha256}=await import('../instrument-package.mjs');
 const catalog=JSON.parse(fs.readFileSync('Library/catalog.json'));
 for(const entry of catalog.packages){
  const bytes=new Uint8Array(fs.readFileSync('Library/'+entry.file));
  assert.equal(await sha256(bytes),entry.sha256);
  const package=readInstrumentPackage(bytes);assert.equal(package.id,entry.id);
  for(const engine of package.engines){
   const module=await compilePackageEngine(engine,catalog.trusted_module_sha256);
   const vm=new WebAssembly.Instance(module,{}).exports;vm._initialize?.();vm.gs_init(32000);
   engine.defaults.forEach((value,address)=>vm.gs_parameter(address,value));
   for(const note of [60,64,67,70])vm.gs_note(note,100);
   const audio=new Float32Array(vm.memory.buffer,vm.gs_render(256),256);
   assert(audio.every(Number.isFinite)&&audio.some(n=>Math.abs(n)>.0001));assert.equal(vm.memory.buffer.byteLength,65536);
   await assert.rejects(()=>compilePackageEngine(engine,[]),/published reference/);
  }
  for(const size of [0,8,12,32,bytes.length-1])assert.throws(()=>readInstrumentPackage(bytes.subarray(0,size)));
  const bad=bytes.slice();bad[8]^=1;assert.throws(()=>readInstrumentPackage(bad),/header/);
  const engine=package.engines[0];engine.module=engine.module.slice();engine.module[10]^=1;
  await assert.rejects(()=>compilePackageEngine(engine,catalog.trusted_module_sha256),/checksum/);
 }
 console.log('PASS browser package decode, module hashes, engine identity, memory budget, audio and malformed data');
})().catch(error=>{console.error(error);process.exitCode=1;});
