import {readInstrumentPackage,compilePackageEngine,sha256} from './instrument-package.mjs';
import {MIDILearn} from './midi-router.mjs';
const $ = id => document.getElementById(id);
const bindings = ['a','w','s','e','d','f','t','g','y','h','u','j','k'];
const noteNames = ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'];
const pages = ['Home','Global','OSC','LFO','Arp'];
const maps = [[3,5,0,1,2,4,17,7],[3,5,0,1,2,4,17,7],[0,1,2,3,5,4,-1,-1],[18,17,7,16,-1,-1,-1,-1],[9,15,11,12,13,8,-1,-1]];
const titles = ['Shape','Attack','Release','Cutoff','Level','Resonance','LFO rate','Depth','Play','Pattern','Arp Hz','Gate','Octaves','Latch','Tempo','Arp rate','Sync','LFO rate','LFO shape'];
const limits = [[0,2],[.005,2],[.01,3],[80,16000],[0,.5],[0,1],[.05,20],[0,1],[0,1],[0,3],[.25,20],[.05,1],[1,4],[0,1],[40,300],[0,8],[0,1],[0,8],[0,4]];
const parameters = [1,.015,.25,5000,.2,0,1,0,0,0,4,.5,1,0,120,3,1,1,1];
const logarithmic = new Set([1,2,3,6,10]);
const discrete = new Set([0,8,9,12,13,15,16,17,18]);
const divisions = ['1/1','1/2','1/4','1/8','1/8T','1/16','1/16T','1/32','1/32T'];
const qualities = [[0,4,7],[0,3,7],[0,4,7,10],[0,3,7,10]];
const voicings = qualities.map(() => ({inversion:0, spread:false}));
const held = new Map(), moduleCache = new Map();
let octave=4, page=0, chord=-1, selectedKnob=0, heldKnob=null, activeVoices=0, lfoStatus=0;
let context, node, analyser, starting, switching=false, faulted=false, instrument='pocket_analog';
let themes=[];
let activePackage=null,packageEngine=0,libraryCatalog=null;
const scene=()=>activePackage&&page>=0?activePackage.engines[packageEngine].scenes[page]:null;
const isGlobal=()=>activePackage?page===-1:page===1;
const pageBindings=()=>scene()?.bindings??maps[isGlobal()?1:page];
const sceneLayout=()=>scene()?.layout??(page===3?1:0);
const waveData = new Float32Array(512);
function normal(address, value=parameters[address]) {
  const [lo,hi]=limits[address];
  return logarithmic.has(address) ? Math.log(value/lo)/Math.log(hi/lo) : (value-lo)/(hi-lo);
}
function denormal(address, fraction) {
  fraction=Math.max(0,Math.min(1,fraction)); const [lo,hi]=limits[address];
  const value=logarithmic.has(address)?lo*Math.pow(hi/lo,fraction):lo+(hi-lo)*fraction;
  return discrete.has(address)?Math.round(value):value;
}
function format(address) {
  const v=parameters[address];
  if(address===0&&activePackage&&!['pocket_analog','fm_bell'].includes(instrument))return String(v);
  if(address===0)return (instrument==='fm_bell'?['1:1','2:1','3:1']:['SINE','SAW','SQUARE'])[v];
  if([1,2].includes(address))return Math.round(v*1000)+' ms';
  if([3,6,10].includes(address))return v>=1000?(v/1000).toFixed(2)+' kHz':v.toFixed(address===3?0:2)+' Hz';
  if([5,7,11].includes(address))return Math.round(v*100)+' %';
  if(address===4)return Math.round(v*200)+' %';
  if(address===9)return ['UP','DOWN','UP/DOWN','RANDOM'][v];
  if([15,17].includes(address))return divisions[v];
  if(address===18)return ['SINE','TRIANGLE','SQUARE','SAW','S&H'][v];
  if([8,13,16].includes(address))return v?'ON':'OFF';
  return String(Math.round(v*10)/10);
}
function addressFor(slot) {const a=pageBindings()[slot];return a===17&&!parameters[16]?6:a;}
function parameter(address,value) {
  parameters[address]=Math.max(limits[address][0],Math.min(limits[address][1],value));
  node?.port.postMessage({type:'parameter',address,value:parameters[address]});
  if(address===14)$('tempo').value=parameters[address];
  renderControls();
}
function bump(slot,delta,fine=false) {
  const address=addressFor(slot);if(address<0)return;
  parameter(address,discrete.has(address)?parameters[address]+delta:denormal(address,normal(address)+delta*(fine?.002:.02)));
}
function renderControls() {
  document.querySelectorAll('.knob').forEach((el,slot)=>{
    const address=addressFor(slot), inactive=address<0;
    el.classList.toggle('unused',inactive);
    el.querySelector('label').textContent=`${slot+1} ${inactive?'UNUSED':scene()?.labels[slot]??titles[address].toUpperCase()}`;
    const input=el.querySelector('input');input.disabled=inactive;
    input.step=inactive?0.002:discrete.has(address)?1/(limits[address][1]-limits[address][0]):0.002;
    if(!inactive){
      const fraction=normal(address);input.value=fraction;
      input.setAttribute('aria-label',`Knob ${slot+1}, ${titles[address]}`);
      input.setAttribute('aria-valuetext',format(address));
      el.style.setProperty('--angle',`${fraction*270}deg`);el.style.setProperty('--rotation',`${fraction*270-135}deg`);
    }
    el.querySelector('output').textContent=inactive?'—':format(address);
  });
  $('screenLabels').replaceChildren();
  pageBindings().forEach((_,slot)=>{
    const address=addressFor(slot);if(address<0)return;
    const el=document.createElement('div');el.textContent=`${slot+1} ${scene()?.labels[slot]??titles[address].toUpperCase()}`;
    const value=document.createElement('b');value.textContent=format(address);el.append(value);$('screenLabels').append(el);
  });
  $('play').textContent=parameters[8]?'− Stop':'− Play';
}
function go(next) {
  if(activePackage&&next>=activePackage.engines[packageEngine].scenes.length)return;
  page=next;document.querySelectorAll('#pages button[data-scene]').forEach(button=>{const selected=Number(button.dataset.scene)===page;button.classList.toggle('selected',selected);button.setAttribute('aria-pressed',selected);});
  const current=scene();
  $('screenTitle').textContent=isGlobal()?'GLOBAL / ESPATCHER':current?current.title+' / '+activePackage.engines[packageEngine].name:pages[page].toUpperCase()+' / AMSYNTH '+(instrument==='fm_bell'?'FM':'ANALOG');
  $('scope').setAttribute('aria-label',['Audio oscilloscope','LFO shape and phase','Parameter values','Parameter levels'][sceneLayout()]);
  $('global').hidden=!isGlobal();$('scope').hidden=isGlobal()||sceneLayout()===2;$('screenLabels').hidden=isGlobal();
  $('packageSubtitle').textContent=current?.subtitle??'';
  renderControls();updatePads();
}
function rebuildPages(){
  $('pages').replaceChildren();
  const entries=activePackage?activePackage.engines[packageEngine].scenes.map(s=>s.title):pages;
  entries.forEach((title,i)=>{const button=document.createElement('button');button.dataset.scene=i;button.textContent=['C','V','B','N','M'][i]+' · '+title;button.onclick=()=>go(i);$('pages').append(button);});
  if(activePackage){const button=document.createElement('button');button.textContent='Global';button.dataset.scene=-1;button.onclick=()=>go(-1);$('pages').append(button);}
}
function updatePads(){
  document.querySelectorAll('#pads .pad').forEach((el,position)=>{
    const note=scene()?.pads[position]??activePackage?.engines[packageEngine].scenes[0].pads[position]??((3-Math.floor(position/4))*4+position%4);
    el.dataset.note=note;el.textContent=noteNames[note%12];el.classList.toggle('root',note%12===0);el.setAttribute('aria-label',`Play ${noteNames[note%12]}`);
  });$('padHint').textContent=activePackage?'Package note layout · lowest note at bottom left':'Chromatic · lowest note at bottom left';renderHeld();
}
async function bundledModule(id) {
  if(moduleCache.has(id))return moduleCache.get(id);
  if(!['pocket_analog','fm_bell'].includes(id))throw new Error('Unknown bundled instrument');
  const response=await fetch(`Firmware/Modules/${id}.gsm5`);if(!response.ok)throw new Error('Module download failed');
  const bytes=await response.arrayBuffer();if(bytes.byteLength>1048576)throw new Error('Oversized module');
  const module=await WebAssembly.compile(bytes);
  if(WebAssembly.Module.imports(module).length)throw new Error('Module imports are not supported');
  const sections=WebAssembly.Module.customSections(module,'gs5');
  if(sections.length!==1)throw new Error('Module manifest missing');
  const manifest=JSON.parse(new TextDecoder().decode(sections[0]));
  if(manifest.abi!==1||manifest.id!==id||manifest.memory_bytes!==262144||manifest.parameter_profile!=='synth-v6')throw new Error('Incompatible reference module');
  moduleCache.set(id,module);return module;
}
async function makeNode(id, override=null, values=parameters) {
  const module=override??await bundledModule(id);
  const next=new AudioWorkletNode(context,'amsynth',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[1],processorOptions:{module,parameters:values}});
  next.port.onmessage=({data})=>{
    if(node!==next)return;
    if(data.type==='fault'){faulted=true;panic();$('status').textContent='Firmware halted: '+data.message;}
    else {activeVoices=data.voices;lfoStatus=data.lfo;$('voices').textContent=`${activeVoices}/4 · OCT ${octave}`;}
  };
  return next;
}
async function start() {
  if(starting)return starting;
  starting=(async()=>{
    if(!window.AudioWorkletNode)throw new Error('AudioWorklet requires a supported browser and HTTPS or localhost');
    if(!context){
      context=new AudioContext({latencyHint:'interactive'});
      await context.audioWorklet.addModule('synth-worklet.js');
      analyser=context.createAnalyser();analyser.fftSize=1024;analyser.connect(context.destination);
    }
    if(faulted&&node){node.disconnect();node.port.close();node=null;faulted=false;}
    if(!node){node=await makeNode(instrument,activePackage?.engines[packageEngine].compiled??null);node.connect(analyser);}
    await context.resume();$('start').textContent='Audio enabled';$('status').textContent=activePackage?'Package engine ready · L opens Library':'AMSynth ready · same DSP as the native app';
  })().catch(error=>{$('status').textContent=error.message;throw error;}).finally(()=>starting=null);
  return starting;
}
async function switchInstrument(id) {
  if(switching)return;switching=true;$('instrument').disabled=true;$('start').disabled=true;
  try {
    panic();
    if(context&&node){const next=await makeNode(id);node.disconnect();node.port.close();node=next;node.connect(analyser);}
    else await bundledModule(id);
    instrument=id;faulted=false;go(page);$('status').textContent='AMSynth '+(id==='fm_bell'?'FM':'Analog')+' selected';
  } catch(error){$('instrument').value=instrument;$('status').textContent=error.message;}
  finally {switching=false;$('instrument').disabled=false;$('start').disabled=false;}
}
function chordNotes(index) {
  if(chord<0)return[index];const notes=[...qualities[chord]],v=voicings[chord];
  for(let i=0;i<v.inversion;i++)notes.push(notes.shift()+12);
  if(v.spread)notes[1]+=12;return notes.map(n=>n+index);
}
function noteOn(source,index,velocity=100,absolute=false) {
  if(!node||context.state!=='running'||faulted){$('status').textContent='Enable audio before playing.';return;}
  if(held.has(source))return;
  const notes=chordNotes(index).map(n=>(absolute?0:12*(octave+1))+n).filter(n=>n<=127);
  const existing=new Set([...held.values()].flat());
  for(const note of notes)if(!existing.has(note))node.port.postMessage({type:'note',note,velocity});
  held.set(source,notes);renderHeld();
}
function noteOff(source) {
  const notes=held.get(source);if(!notes)return;held.delete(source);
  const remaining=new Set([...held.values()].flat());
  for(const note of notes)if(!remaining.has(note))node?.port.postMessage({type:'note',note,velocity:0});renderHeld();
}
function renderHeld(){
  const notes=new Set([...held.values()].flat());
  document.querySelectorAll('[data-note]').forEach(el=>el.classList.toggle('active',notes.has(12*(octave+1)+Number(el.dataset.note))));
}
function panic(){held.clear();node?.port.postMessage({type:'panic'});renderHeld();}
function selectChord(next){panic();chord=chord===next?-1:next;document.querySelectorAll('[data-chord]').forEach(el=>{el.classList.toggle('selected',Number(el.dataset.chord)===chord);el.setAttribute('aria-pressed',Number(el.dataset.chord)===chord);});}
function changeOctave(delta){panic();octave=Math.max(1,Math.min(7,octave+delta));$('octave').textContent=octave;$('voices').textContent=`${activeVoices}/4 · OCT ${octave}`;renderHeld();}
function playable(el,index) {
  el.dataset.note=index;el.setAttribute('aria-label',`Play ${noteNames[index%12]}`);
  el.onpointerdown=e=>{e.preventDefault();el.setPointerCapture(e.pointerId);noteOn('pointer'+e.pointerId,Number(el.dataset.note));};
  for(const event of ['pointerup','pointercancel','lostpointercapture'])el.addEventListener(event,e=>noteOff('pointer'+e.pointerId));
  el.onkeydown=e=>{if(['Enter',' '].includes(e.key)){e.preventDefault();e.stopPropagation();if(!e.repeat)noteOn('button'+index,Number(el.dataset.note));}};
  el.onkeyup=e=>{if(['Enter',' '].includes(e.key)){e.preventDefault();e.stopPropagation();noteOff('button'+index);}};
  el.onblur=()=>noteOff('button'+index);
}
for(let position=0;position<16;position++){
  const index=(3-Math.floor(position/4))*4+position%4,el=document.createElement('button');
  el.className='pad'+(index%12===0?' root':'');el.textContent=noteNames[index%12];playable(el,index);$('pads').append(el);
}
bindings.forEach((key,index)=>{const el=document.createElement('button');el.className='key'+([1,3,6,8,10].includes(index)?' black':'');el.textContent=noteNames[index%12];playable(el,index);$('keys').append(el);});
for(let slot=0;slot<8;slot++){
  const el=document.createElement('div');el.className='knob';el.innerHTML='<label></label><div class="dial"><input type="range" min="0" max="1" step="0.002"></div><output></output>';
  const input=el.querySelector('input'),dial=el.querySelector('.dial');let drag;
  input.oninput=()=>{const a=addressFor(slot);if(a>=0)parameter(a,denormal(a,Number(input.value)));};
  dial.onpointerdown=e=>{const a=addressFor(slot);if(a<0)return;e.preventDefault();input.focus();dial.setPointerCapture(e.pointerId);drag={y:e.clientY,value:normal(a),address:a};};
  dial.onpointermove=e=>{if(!drag)return;parameter(drag.address,denormal(drag.address,drag.value+(drag.y-e.clientY)/(e.shiftKey?900:180)));};
  dial.onpointerup=dial.onpointercancel=dial.onlostpointercapture=()=>{drag=null;};
  dial.ondblclick=()=>{const defaults=[1,.015,.25,5000,.2,0,1,0,0,0,4,.5,1,0,120,3,1,1,1],a=addressFor(slot);if(a>=0)parameter(a,defaults[a]);};
  $('knobs').append(el);
}
window.addEventListener('keydown',e=>{
  if($('firmware').open||$('library').open||e.target.matches('input,select,textarea')||e.metaKey||e.ctrlKey)return;
  if(e.altKey&&/^Digit[1-4]$/.test(e.code)){e.preventDefault();if(!e.repeat)selectChord(Number(e.code.slice(-1))-1);return;}
  const key=e.key.toLowerCase(),index=bindings.indexOf(key);
  if(index>=0){e.preventDefault();if(!e.repeat)noteOn('key'+e.code,index);return;}
  if(key==='l'){e.preventDefault();if(!e.repeat)openLibrary();return;}
  const screen=['c','v','b','n','m'].indexOf(key);if(screen>=0){e.preventDefault();if(!e.repeat)go(screen);return;}
  if(/^[1-8]$/.test(key)){heldKnob=Number(key)-1;selectedKnob=heldKnob;e.preventDefault();return;}
  if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)){
    e.preventDefault();bump(heldKnob??selectedKnob,['ArrowUp','ArrowRight'].includes(e.key)?1:-1,e.shiftKey);return;
  }
  if(key===' '){e.preventDefault();panic();}
  if(!e.repeat){if(key==='z')changeOctave(-1);if(key==='x')changeOctave(1);if(key==='-'){e.preventDefault();parameter(8,1-parameters[8]);}}
});
window.addEventListener('keyup',e=>{noteOff('key'+e.code);if(/^Digit[1-8]$/.test(e.code))heldKnob=null;});
window.addEventListener('blur',()=>{heldKnob=null;panic();});
document.addEventListener('visibilitychange',()=>{if(document.hidden)panic();});
$('start').onclick=()=>start().catch(()=>{});$('instrument').onchange=()=>activePackage?selectPackageEngine(Number($('instrument').value)):switchInstrument($('instrument').value);
$('panic').onclick=panic;$('down').onclick=()=>changeOctave(-1);$('up').onclick=()=>changeOctave(1);
$('play').onclick=()=>parameter(8,1-parameters[8]);$('tempo').onchange=()=>{const value=Number($('tempo').value);if(Number.isFinite(value))parameter(14,value);};
$('manage').onclick=()=>{panic();$('firmware').showModal();};$('done').onclick=()=>$('firmware').close();
document.querySelectorAll('[data-chord]').forEach(el=>el.onclick=()=>selectChord(Number(el.dataset.chord)));
function showVoicing(){const v=voicings[Number($('chordSlot').value)];$('inversion').value=v.inversion;$('spread').checked=v.spread;}
$('chordSlot').onchange=showVoicing;
for(const id of ['inversion','spread'])$(id).onchange=()=>{panic();voicings[Number($('chordSlot').value)]={inversion:Number($('inversion').value),spread:$('spread').checked};};
function color565(n){return `rgb(${Math.round((n>>11&31)*255/31)} ${Math.round((n>>5&63)*255/63)} ${Math.round((n&31)*255/31)})`;}
function setTheme(index){const theme=themes[index];if(!theme)return;for(const role of ['background','surface','text','accent','alternate','inverse','chrome'])document.documentElement.style.setProperty('--'+role,color565(theme[role]));$('theme').value=index;}
fetch('Assets/themes.json').then(r=>r.json()).then(data=>{themes=data.themes;themes.forEach((theme,i)=>{const option=document.createElement('option');option.value=i;option.textContent=theme.name;$('theme').append(option);});setTheme(data.default);}).catch(()=>{});
$('theme').onchange=()=>setTheme(Number($('theme').value));
function draw(){
  requestAnimationFrame(draw);if(document.hidden||isGlobal())return;
  const canvas=$('scope'),ctx=canvas.getContext('2d'),w=canvas.width,h=canvas.height;
  ctx.clearRect(0,0,w,h);const style=getComputedStyle(document.documentElement);ctx.strokeStyle=style.getPropertyValue('--chrome').trim();ctx.globalAlpha=.3;ctx.beginPath();ctx.moveTo(0,h/2);ctx.lineTo(w,h/2);ctx.stroke();ctx.globalAlpha=1;
  if(analyser)analyser.getFloatTimeDomainData(waveData);
  ctx.strokeStyle=style.getPropertyValue('--accent').trim();ctx.lineWidth=2;
  if(sceneLayout()===3){
    ctx.fillStyle=style.getPropertyValue('--accent').trim();
    for(let i=0;i<8;i++){const address=addressFor(i);if(address<0)continue;const height=Math.max(2,normal(address)*(h-20));ctx.fillRect(i*w/8+5,h-10-height,w/8-10,height);}
    return;
  }
  ctx.beginPath();
  for(let i=0;i<256;i++){
    let value=waveData[i]||0;
    if(sceneLayout()===1){const p=i/255,shape=parameters[18];value=shape===0?Math.sin(p*Math.PI*2):shape===1?1-4*Math.abs(p-.5):shape===2?(p<.5?1:-1):shape===3?2*p-1:((lfoStatus&65535)<<16>>16)/32767;value*=parameters[7];}
    const x=i/255*w,y=h/2-value*h*.42;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  }ctx.stroke();
  if(sceneLayout()===1){const x=(lfoStatus>>>16)/65536*w;ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke();}
}
selectChord(-1);rebuildPages();go(0);draw();

const midiLearn=new MIDILearn();let midiAccess;
try{midiLearn.restore(JSON.parse(localStorage.getItem('espatcher.midi.v1')||'[]'));}catch{}
titles.forEach((title,address)=>{const option=document.createElement('option');option.value=address;option.textContent=title+([6,10].includes(address)?" (Hz)":[15,17].includes(address)?" (sync)":"");$('midiTarget').append(option);});$('midiTarget').value=3;
function showMIDI(){const address=Number($('midiTarget').value),map=midiLearn.maps.find(m=>m.address===address);$('midiMapping').textContent=midiLearn.target!==null?'Waiting for a controller CC…':map?`Channel ${map.channel+1} · CC ${map.cc}`:'No mapping for this parameter';}
function saveMIDI(){try{localStorage.setItem('espatcher.midi.v1',JSON.stringify(midiLearn.maps));}catch{}showMIDI();}
$('midiTarget').onchange=showMIDI;
$('midiLearn').onclick=()=>{midiLearn.arm(Number($('midiTarget').value));showMIDI();};
$('midiCancel').onclick=()=>{midiLearn.arm(null);showMIDI();};
$('midiForget').onclick=()=>{midiLearn.forget(Number($('midiTarget').value));saveMIDI();};
$('firmware').addEventListener('close',()=>{midiLearn.arm(null);showMIDI();});
function receiveMIDI(event){
 const data=event.data,status=data[0],type=status&240,channel=status&15,a=data[1],b=data[2];
 const source='midi-'+event.target.id+'-'+channel+'-'+a;
 if(type===144&&b)noteOn(source,a,b,true);
 else if(type===128||(type===144&&!b))noteOff(source);
 else if(type===176&&data.length>=3){
  if(a===120||a===123){panic();return;}
  const result=midiLearn.receive(channel,a,b);if(!result)return;
  if(result.type==='parameter')parameter(result.address,denormal(result.address,result.fraction));
  else if(result.type==='learned')saveMIDI();
  else $('midiMapping').textContent='Mapping table full. Forget a mapping first.';
 }else if(status===250||status===251)parameter(8,1);
 else if(status===252)parameter(8,0);
}
function connectMIDI(){for(const input of midiAccess.inputs.values())input.onmidimessage=receiveMIDI;$('midiStatus').textContent=`${midiAccess.inputs.size} MIDI input(s) connected`;}
$('midiConnect').onclick=async()=>{
 if(!navigator.requestMIDIAccess){$('midiStatus').textContent='Web MIDI is unavailable in this browser. Try a browser with Web MIDI support.';return;}
 try{midiAccess=await navigator.requestMIDIAccess({sysex:false});connectMIDI();midiAccess.onstatechange=()=>{panic();connectMIDI();};}
 catch(error){$('midiStatus').textContent='MIDI access unavailable: '+error.message;}
};showMIDI();

async function catalog(){
 if(!libraryCatalog){const response=await fetch('Library/catalog.json');if(!response.ok)throw new Error('Instrument catalog unavailable');libraryCatalog=await response.json();}
 return libraryCatalog;
}
async function openLibrary(){
 panic();$('library').showModal();$('libraryStatus').textContent='Select an instrument package.';
 try{const data=await catalog();$('libraryItems').replaceChildren();for(const entry of data.packages){
  const button=document.createElement('button');button.textContent=entry.name;
  button.onclick=async()=>{try{const response=await fetch('Library/'+entry.file);if(!response.ok)throw new Error('Package download failed');const bytes=await response.arrayBuffer();if(await sha256(bytes)!==entry.sha256)throw new Error('Package checksum mismatch');await openPackage(bytes);}catch(error){$('libraryStatus').textContent=error.message;}};
  $('libraryItems').append(button);
 }const prototype=document.createElement('a');prototype.href='Instruments/GlitchDrums/';prototype.textContent='GlitchDrums · Full firmware browser prototype (GPL)';$('libraryItems').append(prototype);
 }catch(error){$('libraryStatus').textContent=error.message;}
}
async function openPackage(bytes){
 if(switching)throw new Error('A switch is already in progress');switching=true;$('instrument').disabled=true;$('start').disabled=true;
 try{
  const data=await catalog(),candidate=readInstrumentPackage(bytes);
  $('libraryStatus').textContent='Checking engines…';
  for(const engine of candidate.engines)engine.compiled=await compilePackageEngine(engine,data.trusted_module_sha256);
  const engine=candidate.engines[0];
  const next=context&&node?await makeNode(engine.id,engine.compiled,engine.defaults):null;
  panic();if(next){node.disconnect();node.port.close();node=next;node.connect(analyser);}
  activePackage=candidate;$('instrument').setAttribute('aria-label','Engine');packageEngine=0;parameters.splice(0,19,...engine.defaults);instrument=engine.id;faulted=false;
  $('instrument').replaceChildren();candidate.engines.forEach((engine,i)=>{const option=document.createElement('option');option.value=i;option.textContent=candidate.name+' · '+engine.name;$('instrument').append(option);});
  rebuildPages();go(0);$('tempo').value=parameters[14];$('library').close();$('status').textContent='Loaded '+candidate.name+' from package · L returns to Library';
 }finally{switching=false;$('instrument').disabled=false;$('start').disabled=false;}
}
async function selectPackageEngine(index){
 if(switching||!activePackage||!Number.isInteger(index)||!activePackage.engines[index])return;
 switching=true;$('instrument').disabled=true;
 try{
  const engine=activePackage.engines[index],next=context&&node?await makeNode(engine.id,engine.compiled,engine.defaults):null;
  panic();if(next){node.disconnect();node.port.close();node=next;node.connect(analyser);}
  packageEngine=index;instrument=engine.id;parameters.splice(0,19,...engine.defaults);faulted=false;
  rebuildPages();go(0);$('tempo').value=parameters[14];$('status').textContent='Engine: '+engine.name;
 }catch(error){$('instrument').value=packageEngine;$('status').textContent=error.message;}
 finally{switching=false;$('instrument').disabled=false;}
}
$('openLibrary').onclick=openLibrary;$('libraryDone').onclick=()=>$('library').close();
$('packageFile').onchange=async()=>{
 const file=$('packageFile').files[0];if(!file)return;
 try{if(file.size>4194304)throw new Error('Package larger than 4 MiB');await openPackage(await file.arrayBuffer());}
 catch(error){$('libraryStatus').textContent=error.message;}
 finally{$('packageFile').value='';}
};
