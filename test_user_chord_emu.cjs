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
 function render(frames){let out=[];while(frames){const n=Math.min(1024,frames);assert.strictEqual(x.fm1_render(n,4800000),n,'audio underflow');out.push(...new Float32Array(x.memory.buffer,x.fm1_out_l(),n));frames-=n;}return out;}
 while(x.fm1_ms()<1300&&!x.fm1_halted())x.fm1_render(1024,4800000);
 x.fm1_velocity(4000);
 for (const ps of [5,20]) for (const scenario of [0,1]) {
  const runs=[];
  for(const target of [false,true]) {
  // Reboot for each case so the displayed preset and effects begin in a known state.
  const pp=x.fm1_alloc(pkg.length);new Uint8Array(x.memory.buffer,pp,pkg.length).set(pkg);assert.strictEqual(x.fm1_boot_fwsc(pp,pkg.length,0,0,240),0);
  while(x.fm1_ms()<1300&&!x.fm1_halted())x.fm1_render(1024,20000000);
  for(let i=0;i<ps;i++){x.fm1_key(40,1);render(2205);x.fm1_key(40,0);render(2205);}
  const interval=Number(process.env.ORGAN_INTERVAL_MS||50)*44.1;
  x.fm1_velocity(3000);x.fm1_key(9,1);render(Math.round(interval));x.fm1_key(14,1);render(Math.round(interval));
  if(scenario===0) {x.fm1_key(17,1);render(Math.round(interval));if(target)x.fm1_key(21,1);}
  else {x.fm1_key(17,1);if(target)x.fm1_key(31,1);}
  runs.push(render(22050));
  }
  if(process.env.ORGAN_PCM_DIR) {
   fs.mkdirSync(process.env.ORGAN_PCM_DIR,{recursive:true});
   for(let k=0;k<runs.length;k++)fs.writeFileSync(`${process.env.ORGAN_PCM_DIR}/${ps}-${scenario}-${k}.f32`,Buffer.from(new Float32Array(runs[k]).buffer));
  }
  const a=runs[1], diff=a.map((v,i)=>v-runs[0][i]);let peak=0,step=0;
  for(let i=1;i<a.length;i++){step=Math.max(step,Math.abs(a[i]-a[i-1]));peak=Math.max(peak,Math.abs(a[i]));}
  const f=scenario===0?440:783.9909,h=harmonic(a,4410,.25,f);
  console.log(`preset ${ps} scenario ${scenario}: target ${f}Hz=${h.toFixed(5)} step=${step.toFixed(5)} peak=${peak.toFixed(5)}`);
  for(const start of [4410,11025,17640]) {
   const rms=Math.sqrt(diff.slice(start,start+2205).reduce((s,v)=>s+v*v,0)/2205);
   console.log(`  target contribution at ${Math.round(start/44.1)}ms: rms=${rms.toFixed(6)}`);
   if(rms<0.0001)throw Error('held note missing compared with control');
  }
 }
}
run().catch(e=>{console.error(e);process.exitCode=1});
