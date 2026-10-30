// End-to-end: SMK key contacts -> stock calibration -> voice velocity -> audio.
// Run after building the firmware; uses the same wasm emulator as installer/dist/emu.
const fs = require('fs');
const assert = require('assert');
const wasm = fs.readFileSync('installer/dist/emu/fm1_web.wasm');
const pkg = fs.readFileSync(process.env.ORGAN_TEST_FW || 'vendor/Felucca-smk37/build/felucca-smk37.fwsc');
async function run(keys, later=[], presetSteps=0) {
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
  for(let i=0;i<presetSteps;i++){x.fm1_key(40,1);render(2205);x.fm1_key(40,0);render(2205);}
  x.fm1_velocity(3000);for (const k of keys) x.fm1_key(k,1);
  render(4410);
  for (const k of later) x.fm1_key(k,1);
  const output=render(44100);
  for (const k of [...keys,...later]) x.fm1_key(k,0);
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
 const chord=[14,17,21]; // D4 F4 A4 (C3 = key 0)
 const chordOnly=await run(chord);
 const a3=await run([9]), a3first=await run([9],chord), chordfirst=await run(chord,[9]);
 const a3ref=harmonic(a3,22050,.35,220), before=harmonic(a3first,22050,.35,220), after=harmonic(chordfirst,22050,.35,220);
 console.log(`A3 alone ${a3ref.toFixed(4)}, before chord ${before.toFixed(4)}, after chord ${after.toFixed(4)}`);
 assert(before>a3ref*.5 && after>a3ref*.5,'A3 vanishes when played before or after D minor');
 const plenumA=await run([9],[],5), plenumBoth=await run(chord,[9],5);
 const plenumRef=harmonic(plenumA,22050,.35,220), plenumA3=harmonic(plenumBoth,22050,.35,220);
 console.log(`PLENUM A3 alone ${plenumRef.toFixed(4)}, after chord ${plenumA3.toFixed(4)}`);
 assert(plenumA3>plenumRef*.5,'A3 vanishes on plenum');
 for(const k of [8,9,10,11,12,13,15,16,18,19,20,22,23]) {
  const note=await run([k]), together=await run([...chord,k]);
  const hz=130.81278265*Math.pow(2,k/12); // frequency of this physical key
  const alone=harmonic(note,22050,.35,hz), chordAmp=harmonic(chordOnly,22050,.35,hz), both=harmonic(together,22050,.35,hz);
  console.log(`key ${k}, ${hz.toFixed(1)}Hz: alone ${alone.toFixed(4)}, chord ${chordAmp.toFixed(4)}, with note ${both.toFixed(4)}`);
  assert(both > alone*.30,`key ${k} vanishes over D minor`);
 }
})().catch(e=>{console.error(e);process.exitCode=1});
