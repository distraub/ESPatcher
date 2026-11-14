const assert = require('node:assert/strict');
(async()=>{
 const {MIDILearn}=await import('../midi-router.mjs');const learn=new MIDILearn();
 learn.arm(3);assert.equal(learn.receive(0,74,0).type,'learned');
 assert.equal(learn.receive(1,74,127),null);assert.equal(learn.receive(0,74,127).fraction,1);
 learn.arm(5);learn.receive(0,74,20);assert.equal(learn.maps.length,1);assert.equal(learn.maps[0].address,5);
 learn.arm(7);assert.equal(learn.receive(0,64,127),null);assert.equal(learn.target,7);
 learn.restore([{address:3,channel:0,cc:74},{address:5,channel:0,cc:74},{address:100,channel:0,cc:1}]);
 assert.equal(learn.maps.length,1);learn.forget(3);assert.equal(learn.maps.length,0);
 console.log('PASS web MIDI learn: first-message capture, exact channel, reassignment, reserved CC, validated restore');
})();
