// End-to-end: SMK key contacts -> stock calibration -> voice velocity -> audio.
// Run after building the firmware; uses the same wasm emulator as installer/dist/emu.
const fs = require('fs');
const assert = require('assert');
const wasm = fs.readFileSync('installer/dist/emu/fm1_web.wasm');
const pkg = fs.readFileSync(process.env.ORGAN_TEST_FW || 'vendor/Felucca-smk37/build/felucca-smk37.fwsc');
async function run(contactUs) {
  let x;
  const env = {fm1_jit_compile(ptr, len) {
    try {
      const bytes = new Uint8Array(x.memory.buffer, ptr, len).slice();
      const table = x.__indirect_function_table;
      const fn = new WebAssembly.Instance(new WebAssembly.Module(bytes), {env:{memory:x.memory,table}}).exports.run;
      const slot=table.grow(1); table.set(slot,fn); return slot;
    } catch (_) { return 0; }
  }};
  x=(await WebAssembly.instantiate(wasm,{env})).instance.exports;
  x.fm1_jit(1,16);
  const p=x.fm1_alloc(pkg.length);new Uint8Array(x.memory.buffer,p,pkg.length).set(pkg);
  assert.strictEqual(x.fm1_boot_fwsc(p,pkg.length,0,0,240),0);
 require('./emu_stop_bank.cjs')(x);
  function render(n) {
    const out=[];
    while(n>0) {
      const sz=Math.min(1024,n), got=x.fm1_render(sz,4800000);
      assert.strictEqual(got,sz,'audio underflow');
      out.push(...new Float32Array(x.memory.buffer,x.fm1_out_l(),sz));n-=sz;
    }
    return out;
  }
  while(x.fm1_ms()<1300 && !x.fm1_halted()) x.fm1_render(1024,4800000);
  assert.strictEqual(x.fm1_halted(),0); // boot and finish startup
  x.fm1_velocity(contactUs);x.fm1_key(12,1);
  const output=render(44100);
  x.fm1_key(12,0);
  assert.strictEqual(x.fm1_halted(),0);
  return output;
}
function harmonic(a,start,seconds,hz) {
 const n=Math.round(seconds*44100);let real=0,imag=0;
 for(let i=0;i<n;i++){const w=.5-.5*Math.cos(2*Math.PI*i/(n-1));const ph=2*Math.PI*hz*i/44100;real+=a[start+i]*w*Math.cos(ph);imag-=a[start+i]*w*Math.sin(ph);}
 return Math.hypot(real,imag);
}
function brightness(a,start,end) {let d=0,s=0;for(let i=start+1;i<end;i++){d+=(a[i]-a[i-1])**2;s+=a[i]*a[i];}return Math.sqrt(d/s);}
function rms(a,start,end) {let sum=0;for(let i=start;i<end;i++)sum+=a[i]*a[i];return Math.sqrt(sum/(end-start));}
(async()=>{
 const soft=await run(20000), hard=await run(2000);
 const earlySoft=rms(soft,4410,13230),earlyHard=rms(hard,4410,13230);
 const steadySoft=rms(soft,22050,39690),steadyHard=rms(hard,22050,39690);
 const softColor=brightness(soft,2360,5860),hardColor=brightness(hard,1560,5060);
 const softH=harmonic(soft,1764,.08,1046.4)/harmonic(soft,1764,.08,261.6);
 const hardH=harmonic(hard,970,.08,1046.4)/harmonic(hard,970,.08,261.6);
 console.log(`4th harmonic relative to fundamental: ${softH.toFixed(4)} / ${hardH.toFixed(4)}`);
 assert(hardH>softH*1.15,'hard attack is not measurably brighter');
 console.log(`spectral edge soft/hard ${softColor.toFixed(4)} / ${hardColor.toFixed(4)}`);
 console.log(`contact intervals: 20ms / 2ms; attack RMS ${earlySoft.toFixed(5)} / ${earlyHard.toFixed(5)}; steady RMS ${steadySoft.toFixed(5)} / ${steadyHard.toFixed(5)}; ratio ${(steadyHard/steadySoft).toFixed(2)}x`);
 assert(earlySoft>0 && steadySoft>0,'soft note is silent');
 assert(earlyHard>earlySoft*1.4,'hard attack is not clearly stronger');
 assert(steadyHard>steadySoft*1.3,'velocity no longer affects held note sufficiently');
})().catch(err=>{console.error(err);process.exitCode=1});
