export function readInstrumentPackage(buffer) {
  const bytes=buffer instanceof Uint8Array?buffer:new Uint8Array(buffer),view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  if(bytes.length<16||bytes.length>4194304)throw new Error('Package size outside prototype limits');
  let position=0;
  const take=count=>{if(count<0||count>bytes.length-position)throw new Error('Truncated package');const result=bytes.subarray(position,position+count);position+=count;return result;};
  const u8=()=>take(1)[0];
  const u32=()=>{const at=position;take(4);return view.getUint32(at,true);};
  const text=max=>{const count=u8();if(!count||count>max)throw new Error('Invalid package text length');const data=take(count);if(data.some(n=>n<32||n>126))throw new Error('Package text must be printable ASCII');return new TextDecoder().decode(data);};
  const number=()=>{const at=position;take(4);const n=view.getFloat32(at,true);if(!Number.isFinite(n))throw new Error('Nonfinite package value');return n;};
  const identity=id=>{if(!/^[a-z0-9_-]{1,48}$/.test(id))throw new Error('Invalid package identity');return id;};
  if(new TextDecoder().decode(take(8))!=='ESPKG001'||u32()!==bytes.length)throw new Error('Unsupported package header');
  const result={id:identity(text(48)),name:text(48),version:text(16),engines:[]};
  const count=u8();if(count<1||count>4)throw new Error('Invalid engine count');
  const limits=[[0,2],[.005,2],[.01,3],[80,16000],[0,.5],[0,1],[.05,20],[0,1],[0,1],[0,3],[.25,20],[.05,1],[1,4],[0,1],[40,300],[0,8],[0,1],[0,8],[0,4]];
  const discrete=new Set([0,8,9,12,13,15,16,17,18]);
  for(let e=0;e<count;e++){
    const engine={id:identity(text(48)),name:text(48),memory:u32(),defaults:[],scenes:[]};
    if(result.engines.some(other=>other.id===engine.id))throw new Error('Duplicate engine identity');
    if(![65536,131072,262144].includes(engine.memory))throw new Error('Unsupported engine memory');
    for(let p=0;p<19;p++){
      const value=number(),[lo,hi]=limits[p];
      // Compare against f32 limits, exactly as the device decoder does.
      if(value<Math.fround(lo)||value>Math.fround(hi)||(discrete.has(p)&&!Number.isInteger(value)))throw new Error('Invalid parameter default');
      engine.defaults.push(value);
    }
    if(engine.defaults[8]!==0)throw new Error('Package transport must default to stopped');
    const sceneCount=u8();if(sceneCount<1||sceneCount>4)throw new Error('Invalid scene count');
    for(let s=0;s<sceneCount;s++){
      const scene={title:text(24),subtitle:text(48),layout:u8(),bindings:[...take(8)],labels:[],pads:[]};
      if(scene.layout>3||scene.bindings.some(n=>n!==255&&n>=19))throw new Error('Invalid scene or parameter binding');
      scene.bindings=scene.bindings.map(n=>n===255?-1:n);
      for(let i=0;i<8;i++)scene.labels.push(text(8));
      scene.pads=[...take(16)];if(scene.pads.some(n=>n>48))throw new Error('Invalid pad offset');
      engine.scenes.push(scene);
    }
    const length=u32();if(length<8||length>1048576)throw new Error('Invalid module size');
    engine.digest=[...take(32)].map(n=>n.toString(16).padStart(2,'0')).join('');engine.module=take(length);
    result.engines.push(engine);
  }
  if(position!==bytes.length)throw new Error('Unexpected trailing package data');
  return result;
}
export async function sha256(bytes){return [...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(n=>n.toString(16).padStart(2,'0')).join('');}
export async function compilePackageEngine(engine, trustedHashes) {
  const hash=await sha256(engine.module);
  if(hash!==engine.digest)throw new Error('Engine checksum mismatch');
  if(!trustedHashes.includes(hash))throw new Error('Browser prototype only runs the published reference DSP hashes. Try this package in the fuel-limited device experiment.');
  const module=await WebAssembly.compile(engine.module);
  if(WebAssembly.Module.imports(module).length)throw new Error('Module imports are not supported');
  const sections=WebAssembly.Module.customSections(module,'gs5');if(sections.length!==1)throw new Error('Missing engine manifest');
  const manifest=JSON.parse(new TextDecoder().decode(sections[0]));
  if(manifest.abi!==1||manifest.format!==1||manifest.id!==engine.id||manifest.memory_bytes!==engine.memory||manifest.parameter_profile!=='synth-v6')throw new Error('Package and engine manifest disagree');
  return module;
}
