// Parameter-address mappings, independent of the current page or knob slot.
export const allowedCC = cc => Number.isInteger(cc) && cc >= 0 && cc < 120 && ![0,6,32,38,64,96,97,98,99,100,101].includes(cc);
export class MIDILearn {
  constructor() { this.maps=[]; this.target=null; }
  arm(address) { this.target=Number.isInteger(address)&&address>=0&&address<19?address:null; }
  forget(address) { this.maps=this.maps.filter(m=>m.address!==address); }
  restore(maps) {
    this.maps=[];
    if(!Array.isArray(maps))return;
    for(const m of maps){
      if(this.maps.length>=16)break;
      if(!m||!Number.isInteger(m.address)||m.address<0||m.address>=19||!Number.isInteger(m.channel)||m.channel<0||m.channel>15||!allowedCC(m.cc))continue;
      if(this.maps.some(n=>n.address===m.address||(n.channel===m.channel&&n.cc===m.cc)))continue;
      this.maps.push({address:m.address,channel:m.channel,cc:m.cc});
    }
  }
  receive(channel,cc,value) {
    if(!Number.isInteger(channel)||channel<0||channel>15||!allowedCC(cc)||!Number.isInteger(value)||value<0||value>127)return null;
    if(this.target!==null){
      const address=this.target;
      const remaining=this.maps.filter(m=>m.address!==address&&!(m.channel===channel&&m.cc===cc));
      if(remaining.length>=16)return {type:'full'};
      this.maps=[...remaining,{address,channel,cc}];this.target=null;
      return {type:'learned',address,channel,cc};
    }
    const map=this.maps.find(m=>m.channel===channel&&m.cc===cc);
    return map?{type:'parameter',address:map.address,fraction:value/127}:null;
  }
}
