const $ = id => document.getElementById(id);
const bindings = ['a','w','s','e','d','f','t','g','y','h','u','j','k'];
const names = ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B','C'];
const black = new Set([1,3,6,8,10]);
let octave=4, context, oscillator, filter, envelope, master, starting;
const held = new Map();
const cutoff = () => 80 * Math.pow(200, Number($('cutoff').value));
function refreshValues(){
  for(const id of ['attack','release']) $(id+'Value').textContent=Math.round(Number($(id).value)*1000)+' ms';
  $('cutoffValue').textContent=Math.round(cutoff())+' Hz';
  $('volumeValue').textContent=Math.round(Number($('volume').value)*200)+'%';
}
function updateSound(){
  refreshValues(); if(!context)return;
  oscillator.type=$('wave').value;
  filter.frequency.setTargetAtTime(cutoff(),context.currentTime,0.015);
  master.gain.setTargetAtTime(Number($('volume').value),context.currentTime,0.015);
}
async function start(){
  if(starting)return starting;
  starting=(async()=>{
    if(!context){
      context=new AudioContext({latencyHint:'interactive'});
      oscillator=context.createOscillator();filter=context.createBiquadFilter();
      envelope=context.createGain();master=context.createGain();
      filter.type='lowpass';filter.Q.value=0.707;envelope.gain.value=0;
      oscillator.connect(filter).connect(envelope).connect(master).connect(context.destination);
      updateSound();oscillator.start();
      context.onstatechange=()=>{const on=context.state==='running';$('audioState').textContent=on?'ON':'PAUSED';$('lamp').classList.toggle('on',on);};
    }
    await context.resume();$('start').textContent='Audio enabled';$('status').textContent='Play with your computer keyboard or the on-screen keys.';
  })().finally(()=>starting=null);
  return starting;
}
function ramp(value,seconds){const now=context.currentTime;envelope.gain.cancelAndHoldAtTime(now);envelope.gain.linearRampToValueAtTime(value,now+seconds);}
function renderHeld(){
  document.querySelectorAll('.key').forEach((key,i)=>key.classList.toggle('active',[...held.values()].some(n=>n%12===i%12 && n===12*(octave+1)+i)));
  if(!held.size){$('note').textContent='—';return;}
  const midi=[...held.values()].at(-1);$('note').textContent=names[midi%12]+' '+(Math.floor(midi/12)-1);
}
function noteOn(source,index){
  if(!context||context.state!=='running'){ $('status').textContent='Click Enable audio first.';return; }
  if(held.has(source))return;
  const midi=12*(octave+1)+index;held.set(source,midi);
  oscillator.frequency.setTargetAtTime(440*Math.pow(2,(midi-69)/12),context.currentTime,0.003);
  ramp(1,Number($('attack').value));renderHeld();
}
function noteOff(source){
  if(!held.delete(source))return;
  if(held.size){const midi=[...held.values()].at(-1);oscillator.frequency.setTargetAtTime(440*Math.pow(2,(midi-69)/12),context.currentTime,0.003);}
  else if(context)ramp(0,Number($('release').value));renderHeld();
}
function panic(){held.clear();if(context)ramp(0,0.01);renderHeld();}
function changeOctave(delta){panic();octave=Math.max(1,Math.min(7,octave+delta));$('octave').textContent='OCT '+octave;}
let whites=0;
bindings.forEach((binding,i)=>{
 const key=document.createElement('button');key.className='key '+(black.has(i)?'black':'white');key.textContent=binding.toUpperCase();key.setAttribute('aria-label',names[i]+' — '+binding.toUpperCase());
 if(black.has(i))key.style.left=(whites/8*100-3.75)+'%';else whites++;
 key.addEventListener('pointerdown',e=>{e.preventDefault();key.setPointerCapture(e.pointerId);noteOn('pointer'+e.pointerId,i);});
 for(const event of ['pointerup','pointercancel','lostpointercapture'])key.addEventListener(event,e=>noteOff('pointer'+e.pointerId));
 key.addEventListener('keydown',e=>{if(['Enter',' '].includes(e.key)){e.preventDefault();noteOn('button'+i,i);}});
 key.addEventListener('keyup',e=>{if(['Enter',' '].includes(e.key)){e.preventDefault();noteOff('button'+i);}});
 key.addEventListener('blur',()=>noteOff('button'+i));$('keys').append(key);
});
window.addEventListener('keydown',e=>{
 if(e.target.matches('input,select')||e.metaKey||e.ctrlKey||e.altKey)return;
 const k=e.key.toLowerCase(),i=bindings.indexOf(k);
 if(i>=0){e.preventDefault();if(!e.repeat)noteOn('key'+k,i);}
 if(k===' '){e.preventDefault();panic();}if(!e.repeat&&k==='z')changeOctave(-1);if(!e.repeat&&k==='x')changeOctave(1);
});
window.addEventListener('keyup',e=>noteOff('key'+e.key.toLowerCase()));
window.addEventListener('blur',panic);document.addEventListener('visibilitychange',()=>{if(document.hidden)panic();});
$('start').onclick=()=>start().catch(error=>{$('status').textContent='Audio could not start: '+error.message;});
$('panic').onclick=panic;$('down').onclick=()=>changeOctave(-1);$('up').onclick=()=>changeOctave(1);
for(const id of ['wave','attack','release','cutoff','volume'])$(id).addEventListener('input',updateSound);
refreshValues();
