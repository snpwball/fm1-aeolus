// End-to-end: SMK key contacts -> stock calibration -> voice velocity -> audio.
// Run after building the firmware; uses the same wasm emulator as installer/dist/emu.
const fs = require('fs');
const assert = require('assert');
const wasm = fs.readFileSync('installer/dist/emu/fm1_web.wasm');
const pkg = fs.readFileSync(process.env.ORGAN_TEST_FW || 'vendor/Felucca-smk37/build/felucca-smk37.fwsc');
function harmonic(a,start,seconds,hz) {
 const n=Math.round(seconds*44100);let real=0,imag=0;
 for(let i=0;i<n;i++){const w=.5-.5*Math.cos(2*Math.PI*i/(n-1));const ph=2*Math.PI*hz*i/44100;real+=a[start+i]*w*Math.cos(ph);imag-=a[start+i]*w*Math.sin(ph);}
 return Math.hypot(real,imag);
}
async function run() {
 let x; const env={fm1_jit_compile(ptr,len){try {const b=new Uint8Array(x.memory.buffer,ptr,len).slice();const table=x.__indirect_function_table;const fn=new WebAssembly.Instance(new WebAssembly.Module(b),{env:{memory:x.memory,table}}).exports.run;const slot=table.grow(1);table.set(slot,fn);return slot;}catch(_){return 0;}}};
 x=(await WebAssembly.instantiate(wasm,{env})).instance.exports;x.fm1_jit(1,16);
 const p=x.fm1_alloc(pkg.length);new Uint8Array(x.memory.buffer,p,pkg.length).set(pkg);assert.strictEqual(x.fm1_boot_fwsc(p,pkg.length,0,0,240),0);
 require('./emu_stop_bank.cjs')(x);
 function render(frames){let out=[];while(frames){const n=Math.min(1024,frames);assert.strictEqual(x.fm1_render(n,4800000),n,'audio underflow');out.push(...new Float32Array(x.memory.buffer,x.fm1_out_l(),n));frames-=n;}return out;}
 while(x.fm1_ms()<1300&&!x.fm1_halted())x.fm1_render(1024,4800000);
 x.fm1_velocity(4000);
 for(const high of [33,35]) for(const ms of [10,25,50,90]) for(const order of [0,1]) {
  const keys=order?[high,7]:[7,high];
  for(let j=0;j<4;j++) {
   x.fm1_key(keys[0],1);render(Math.round(ms*44.1));x.fm1_key(keys[1],1);
   const a=render(13230);const lo=harmonic(a,4410,.16,195.9977);const hi=harmonic(a,4410,.16,130.81278265*Math.pow(2,high/12));
   if(lo<.1||hi<.1) throw Error(`missing at high=${high}, delay=${ms}ms order=${order} cycle=${j}: G3=${lo} high=${hi}`);
   x.fm1_key(keys[0],0);x.fm1_key(keys[1],0);render(8820);
  }
  console.log(`G3 + ${high===33?'A5':'B5'}, ${ms}ms ${order?'high first':'G first'}: ok`);
 }
}
run().catch(e=>{console.error(e);process.exitCode=1});
