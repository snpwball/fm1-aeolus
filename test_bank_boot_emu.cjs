const fs=require('fs');
const assert=require('assert/strict');
const wasm=fs.readFileSync('installer/dist/emu/fm1_web.wasm');
const pkg=fs.readFileSync('vendor/Felucca-smk37/build/felucca-smk37.fwsc');
async function run(kind) {
 let x;
 const env={fm1_jit_compile(ptr,len){
  try{
   const b=new Uint8Array(x.memory.buffer,ptr,len).slice(),table=x.__indirect_function_table;
   const f=new WebAssembly.Instance(new WebAssembly.Module(b),{env:{memory:x.memory,table}}).exports.run;
   const slot=table.grow(1);table.set(slot,f);return slot;
  }catch{return 0;}
 }};
 x=(await WebAssembly.instantiate(wasm,{env})).instance.exports;x.fm1_jit(1,16);
 const p=x.fm1_alloc(pkg.length);new Uint8Array(x.memory.buffer,p,pkg.length).set(pkg);
 assert.equal(x.fm1_boot_fwsc(p,pkg.length,0,0,240),0);
 if(kind!=='missing')require('./emu_stop_bank.cjs')(x);
 if(kind==='corrupt'){
  const data=Buffer.alloc(4100),at=0xc3000;
  data.writeUInt32LE(at);
  data.set(new Uint8Array(x.memory.buffer,x.fm1_flash()+at,4096),4);data[123]^=1;
  const p=x.fm1_alloc(data.length);new Uint8Array(x.memory.buffer,p,data.length).set(data);
  assert.equal(x.fm1_flash_restore(p,data.length),0);
 }
 for(let warm=0;x.fm1_ms()<1300&&!x.fm1_halted()&&warm<200;warm++)x.fm1_render(1024,20000000);
 assert.equal(x.fm1_halted(),0);assert(x.fm1_ms()>=1300);
 x.fm1_velocity(3000);x.fm1_key(12,1);
 let peak=0;
 for(let n=0;n<50;n++){
  assert.equal(x.fm1_render(1024,4800000),1024);
  for(const v of new Float32Array(x.memory.buffer,x.fm1_out_l(),1024))peak=Math.max(peak,Math.abs(v));
 }
 assert.equal(x.fm1_halted(),0);
 if(kind==='valid')assert(peak>0.001);else assert(peak<1e-7,`${kind} bank played invalid wave data`);
 console.log(`${kind} bank: peak ${peak.toFixed(6)}`);
}
(async()=>{for(const kind of ['missing','corrupt','valid'])await run(kind);})().catch(e=>{console.error(e);process.exitCode=1;});
