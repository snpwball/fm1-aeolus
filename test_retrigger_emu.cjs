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
 for(const pause of [5,10,20,50,100]) {
  for(let j=0;j<4;j++) {
   x.fm1_key(9,1);render(4410);x.fm1_key(14,1);render(4410);x.fm1_key(17,1);render(4410);
   x.fm1_key(9,0);render(Math.round(pause*44.1));x.fm1_key(9,1);
   const a=render(13230);const hz=harmonic(a,4410,.16,220);
   let diff=0,peak=0;for(let i=1;i<a.length;i++){diff=Math.max(diff,Math.abs(a[i]-a[i-1]));peak=Math.max(peak,Math.abs(a[i]));}
   console.log(`pause=${pause}ms cycle=${j} A3=${hz.toFixed(4)} maxstep=${diff.toFixed(5)} peak=${peak.toFixed(5)}`);
   if(hz<.1)throw Error('missing A3');
   for(const k of [9,14,17])x.fm1_key(k,0);render(13230);
  }
 }
}
run().catch(e=>{console.error(e);process.exitCode=1});
