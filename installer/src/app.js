// Installer transport is Felucca's GPL-3.0-only fm1ota.js / fm1pkg.js.
import {Updater, sameModel} from './fm1ota.js';
import {Package, productOf, logicalImage, sha256hex} from './fm1pkg.js';
import {installStopBanks, stopBankStatus} from './install_stop_bank.mjs';
const $ = id => document.getElementById(id);
const button=$('installButton'), status=$('status'), progress=$('progress'), log=$('log');
let image=null, product='', busy=false, bankManifest=null, bankImages=[];
const message=x=>{status.textContent=x};
const updateButton=()=>{button.disabled=busy||!image||!$('model').checked||!$('backup').checked};
for(const id of ['model','backup']) $(id).addEventListener('change',updateButton);
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
try {
 const manifest=await fetch('./build-info.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error('No build info');return r.json()});
 const response=await fetch('./organ-smk37-pro.fwsc',{cache:'no-store'});if(!response.ok)throw Error('Package not found');
 const bytes=new Uint8Array(await response.arrayBuffer());const sha=await sha256hex(bytes);
 if(sha!==manifest.sha256)throw Error('Checksum does not match');
 product=productOf(bytes);if(product!=='SMK-37 Pro_900'||product!==manifest.product)throw Error('Package is not for SMK-37 Pro');
 new Package(bytes);
 bankManifest=await fetch('./organ-banks.json',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error('Stop bank manifest unavailable');return r.json()});
 if(bankManifest.firmware_sha256!==sha)throw Error('Firmware and stop bank manifests do not match');
 bankImages=await Promise.all(bankManifest.banks.map(async bank=>{
  const r=await fetch('./'+bank.file,{cache:'no-store'});if(!r.ok)throw Error('Stop bank unavailable');
  const data=new Uint8Array(await r.arrayBuffer());
  if(data.length!==bank.size || await sha256hex(data)!==bank.sha256)throw Error('Stop bank checksum does not match');
  return data;
 }));
 image=logicalImage(bytes);
 $('package').textContent=`${product} · ${(bytes.length/1024).toFixed(0)} KB · SHA-256 ${sha}`;
 message('File checked. Confirm the steps above.');updateButton();
} catch(e){message('Check failed: '+e.message);$('package').textContent='Package unavailable';}
button.addEventListener('click',async()=>{
 if(busy||!image)return;if(!navigator.requestMIDIAccess){message('Chrome or Edge with Web MIDI is required.');return;}
 busy=true;updateButton();log.textContent='';
 try {
  const midi=await navigator.requestMIDIAccess({sysex:true});
  const up=new Updater(midi,x=>{log.textContent+=x+'\n'});
  const phases={start:'Keyboard found. Preparing…',verify:'Keyboard is checking the package…',loader:'Entering update mode…',write:'Writing firmware…',reboot:'Waiting for restart…',done:'Firmware ready. Checking stop banks…'};
  const step=(name,value)=>{message(phases[name]||name);if(name==='verify')progress.value=Math.min(20,Number(value)/74*20);if(name==='write')progress.value=20+Math.min(45,Number(value)/2483*45);if(name==='done')progress.value=65;log.textContent+=`${name} ${value??''}\n`;};
  const current=await up.find();
  if(!current)throw Error('SMK-37 Pro not found');
  if(!sameModel(current.id.model,product.split('_')[0])){current.link.close();throw Error('Connected keyboard is not an SMK-37 Pro');}
  if(/^ota-/i.test(current.id.model)){
   current.link.close();const ok=await up.resume(image,step,product);if(!ok)throw Error('Resume did not finish');
  } else {
   let matched=false;
   try{await stopBankStatus(current.link,bankManifest,1);matched=true;}catch(e){log.textContent+='Firmware update required: '+e.message+'\n';}
   current.link.close();
   if(!matched)await up.install(image,product,step);
  }
  const device=await up.waitFor(id=>id.text===product,40000);
  if(!device)throw Error('Firmware installed; reconnect and click Install to finish the stop banks');
  try{
   await installStopBanks(device.link,bankManifest,bankImages,(slot,value)=>{
    message(`Stop bank ${slot+1}/2: ${value}%`);progress.value=65+(slot+value/100)*17.5;
   });
  }finally{device.link.close();}
  progress.value=100;message('Install finished. Firmware and both stop banks checked.');

 } catch(e){message('Not finished: '+(e.message||String(e)));log.textContent+=String(e.stack||e)+'\n';}
 finally {busy=false;updateButton();}
});

$('diagnoseButton').addEventListener('click',async()=>{
 if(busy)return;
 if(!navigator.requestMIDIAccess){message('Chrome or Edge with Web MIDI is required.');return;}
 busy=true;updateButton();$('diagnoseButton').disabled=true;log.textContent='';
 try {
  message('Checking MIDI ports and keyboard identity…');
  const midi=await navigator.requestMIDIAccess({sysex:true});
  const ins=[...midi.inputs.values()].filter(p=>p.state!=='disconnected');
  const outs=[...midi.outputs.values()].filter(p=>p.state!=='disconnected');
  log.textContent='MIDI inputs: '+(ins.map(p=>p.name).join(', ')||'(none)')+'\nMIDI outputs: '+(outs.map(p=>p.name).join(', ')||'(none)')+'\n';
  const up=new Updater(midi,x=>{log.textContent+=x+'\n'});
  const found=await up.find();
  if(found){const name=found.id.text;found.link.close();message('Keyboard answered: '+name+'. No firmware was written.');}
  else message('No keyboard answered. Open Details for the MIDI port names; no firmware was written.');
  document.querySelector('details').open=true;
 } catch(e){message('MIDI check failed: '+(e.message||String(e)));log.textContent+=String(e.stack||e)+'\n';document.querySelector('details').open=true;}
 finally {busy=false;updateButton();$('diagnoseButton').disabled=false;}
});
