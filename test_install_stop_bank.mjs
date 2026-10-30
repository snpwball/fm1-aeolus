import assert from 'node:assert/strict';
import fs from 'node:fs';
import {spawn} from 'node:child_process';
import readline from 'node:readline';
import {installStopBanks, stopBankStatus} from './install_stop_bank.mjs';

const manifest=JSON.parse(fs.readFileSync('build/organ-banks.json'));
const images=manifest.banks.map(b=>new Uint8Array(fs.readFileSync('build/'+b.file)));
const child=spawn('build/test_bank_editor',[],{env:{...process.env,ORGAN_BANK_WIRE:'1'},stdio:['pipe','pipe','inherit']});
const queued=[];let wake;
const exit=new Promise(resolve=>child.on('exit',resolve));
const lines=readline.createInterface({input:child.stdout});
let command=0,dropReply=true,interrupt=true,slot0Begins=0;
lines.on('line',line=>{
  const bytes=line.trim().split(/\s+/).filter(Boolean).map(s=>parseInt(s,16));
  if(dropReply && command===12) {dropReply=false;return;}
  queued.push(bytes);if(wake){const fn=wake;wake=null;fn();}
});
const link={
  lost:false,
  send(bytes){
    command=bytes[4];
    if(command===11 && bytes[5]===0)slot0Begins++;
    if(command===12 && bytes[5]===1 && interrupt){interrupt=false;return false;}
    child.stdin.write(bytes.map(b=>b.toString(16)).join(' ')+'\n');return true;
  },
  async read(ms){
    if(!queued.length)await new Promise(resolve=>{const timer=setTimeout(()=>{wake=null;resolve();},ms);wake=()=>{clearTimeout(timer);resolve();};});
    return queued.shift()||null;
  }
};
try {
  assert.equal((await stopBankStatus(link,manifest)).valid,0);
  await assert.rejects(installStopBanks(link,manifest,images),/disconnected/);
  assert.equal((await stopBankStatus(link,manifest)).valid,1);
  await installStopBanks(link,manifest,images);
  assert.equal((await stopBankStatus(link,manifest)).ready,true);
  assert.equal(slot0Begins,1);
  await installStopBanks(link,manifest,images);
  assert.equal(slot0Begins,1);
  console.log('Browser bank uploader passed: dropped reply, interrupted second slot, resume and verification');
} finally {child.stdin.end();}
assert.equal(await exit,0);
