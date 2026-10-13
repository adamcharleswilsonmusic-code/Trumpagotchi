import fs from 'node:fs';
import assert from 'node:assert/strict';
let mem;
const wasi=new Proxy({clock_time_get:(id,p,out)=>{new DataView(mem.buffer).setBigUint64(out,0n,true);return 0;},fd_write:(fd,iov,n,out)=>{new DataView(mem.buffer).setUint32(out,0,true);return 0;}},{get:(t,k)=>k in t?t[k]:()=>0});
const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi,env:new Proxy({},{get:()=>()=>0})});
const e=instance.exports;mem=e.memory;if(e._initialize)e._initialize();e.web_boot();e.web_master(4096);
function render(n){let peak=0;while(n){const k=Math.min(256,n);e.web_render(k);for(const x of new Float32Array(mem.buffer,e.web_out_l(),k)){assert(Number.isFinite(x));assert(Math.abs(x)<=1);peak=Math.max(peak,Math.abs(x));}n-=k;}return peak;}
assert.equal(render(44100),0,'idle silent');
assert.equal(e.web_lit_keys(),(1<<1)|(1<<3)|(1<<5)|(1<<8),'four steady care lights');
assert.equal(e.web_lit_buttons(),(1<<5)|(1<<6)|(1<<7)|(1<<10),'usable buttons stay lit');
e.web_buttons(1<<10);render(4410);e.web_buttons(0);render(4410);
assert.equal(render(44100*65),0,'paused room is silent');
e.web_buttons(1<<10);render(4410);e.web_buttons(0);render(4410);
e.web_buttons(1<<5);render(4410);e.web_buttons(0);render(4410);
assert(e.web_lit_buttons() & (1<<1),'Select lights in Global');
e.web_buttons(1<<6);render(4410);e.web_buttons(0);render(4410);
assert.equal(e.web_lit_buttons() & (1<<1),0,'Select off outside Global');
e.web_keys(1<<1);assert(render(4410)>0.01,'FEED care key plays its recorded reaction');e.web_keys(0);
e.web_keys(1);assert(render(44100)>0.01,'white key sounds');e.web_keys(0);
render(44100*6);
e.web_buttons(1<<6);render(4410);e.web_buttons(0);render(4410); // HOME status hides idle footsteps
assert(render(44100)<0.002,'notes and accompaniment end');
e.web_buttons(1<<7);render(4410);e.web_buttons(0);render(4410);
assert(e.web_store_writes()>0,'manual save');
const n=e.web_store_len(0);assert(n>0);const bytes=Buffer.from(new Uint8Array(mem.buffer,e.web_store(0),n));
e.web_boot();render(4410);assert.equal(e.web_store_len(0),n);assert.deepEqual(Buffer.from(new Uint8Array(mem.buffer,e.web_store(0),n)),bytes);
assert([...new Uint16Array(mem.buffer,e.web_fb(),240*240)].some(x=>x!==0),'screen rendered');
// Load an exhausted pet through the real browser storage ABI.
const input=new Uint8Array(mem.buffer,e.web_store(0),n);input.fill(0,16,19);input[19]=100;
e.web_boot();assert(render(44100*2)>0.01,'game-over synthesized sound');
const exhausted=Buffer.from(new Uint8Array(mem.buffer,e.web_store(0),n));
assert.deepEqual([...exhausted.subarray(16,20)],[0,0,0,100]);
assert.equal(render(44100),0,'game over is quiet after its one-shot');
e.web_buttons(1<<6);render(256);e.web_buttons(0);render(256);
const restarted=Buffer.from(new Uint8Array(mem.buffer,e.web_store(0),n));
assert.deepEqual([...restarted.subarray(16,20)],[100,100,100,100],'button restarts full pet');
assert.equal(restarted[26],3,'full burger tray');
// Night sleep and a nap crossing dusk both persist full energy at dawn.
for(const startMinute of [1260,1259]){
    const state=new Uint8Array(mem.buffer,e.web_store(0),n);
    state[18]=10;state[20]=1;state[22]=0;state[24]=startMinute&255;state[25]=startMinute>>8;
    e.web_boot();render(44100*8);
    e.web_buttons(1<<7);render(256);e.web_buttons(0);render(256);
    const morning=new Uint8Array(mem.buffer,e.web_store(0),n);
    assert.equal(morning[18],100,'sleep through dawn restores full energy');
    assert.equal(morning[20],0,'awake at dawn');
}
console.log('Browser WASM PASS: silent boot, voiced care, playable white key, bounded samples, released audio ends, saved payload survives boot, screen drawn, game-over one-shot, full restart, full-energy dawn and nap crossing dusk.');
