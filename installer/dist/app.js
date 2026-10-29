// Installer transport is Felucca's GPL-3.0-only fm1ota.js / fm1pkg.js.
import {Updater} from './fm1ota.js';
import {Package, productOf, logicalImage, sha256hex} from './fm1pkg.js';
const $ = id => document.getElementById(id);
const button=$('installButton'), status=$('status'), progress=$('progress'), log=$('log');
let image=null, product='', busy=false;
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
 new Package(bytes);image=logicalImage(bytes);
 $('package').textContent=`${product} · ${(bytes.length/1024).toFixed(0)} KB · SHA-256 ${sha}`;
 message('File checked. Confirm the steps above.');updateButton();
} catch(e){message('Check failed: '+e.message);$('package').textContent='Package unavailable';}
button.addEventListener('click',async()=>{
 if(busy||!image)return;if(!navigator.requestMIDIAccess){message('Chrome or Edge with Web MIDI is required.');return;}
 busy=true;updateButton();log.textContent='';
 try {
  const midi=await navigator.requestMIDIAccess({sysex:true});
  const up=new Updater(midi,x=>{log.textContent+=x+'\n'});
  const phases={start:'Keyboard found. Preparing…',verify:'Keyboard is checking the package…',loader:'Entering update mode…',write:'Writing firmware…',reboot:'Waiting for restart…',done:'Install finished. The keyboard came back with the expected name.'};
  const step=(name,value)=>{message(phases[name]||name);if(name==='verify')progress.value=Math.min(20,Number(value)/74*20);if(name==='write')progress.value=20+Math.min(75,Number(value)/1167*75);if(name==='done')progress.value=100;log.textContent+=`${name} ${value??''}\n`;};
  const ota=await up.find(id=>/^ota-/i.test(id.model));
  if(ota){ota.link.close();const ok=await up.resume(image,step,product);if(!ok)throw Error('Resume did not finish');}
  else await up.install(image,product,step);
 } catch(e){message('Not finished: '+(e.message||String(e)));log.textContent+=String(e.stack||e)+'\n';}
 finally {busy=false;updateButton();}
});
