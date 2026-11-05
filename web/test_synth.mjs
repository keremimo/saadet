// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Real firmware capability/archive fixtures and an independent wire peer.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {BACKUP_IDS,bkManifest,bkU32,bkR32,bkPack,bkUnpack,bkCrc,readBackup,captureBackup,restoreBackup} from './fm1backup.js';
const html=readFileSync(new URL('editor.html',import.meta.url),'utf8');
const proto=html.split('/*PROTO-BEGIN*/')[1].split('/*PROTO-END*/')[0];
const E=vm.runInNewContext(`${proto};({parse,CMD,engineOrder,FM6,CZ,P5,J106,HY,bank,cleanPatch,libraryFile,readSysexLibrary})`,{setTimeout,clearTimeout,setInterval,clearInterval,console});
const info=E.parse[E.CMD.INFO](Array.from(readFileSync('build/host/synth-info.bin')));
assert(info.synthOnly && info.ntrk===2 && info.nstep===0);
assert(info.fm6 && info.cz && info.prophet && info.backupCaps===3);
assert.equal(info.uiCaps&16,0);assert.equal(info.shared.total,384);assert.equal(info.native,null);
assert.equal(E.engineOrder(info.engines).map(i=>info.engines[i]).join(),'PROPHET,FM6,CZ-1,JUNO-106,HYDRA');
assert(info.juno&&info.juno.engine===21&&info.juno.namespace===22&&info.juno.fields===26&&info.namespace[21]===22);
const jf=[240,65,48,2,9,...Array.from({length:16},(_,i)=>i*7),82,0x1f,247],jt=E.J106.decode(jf);
assert.equal(E.J106.name(jt),'PATCH A22');assert.deepEqual(Array.from(E.J106.sysex(jt,2)),jf);
assert.deepEqual([18,19,20,21,22,23,24,25].map(id=>E.J106.get(jt,id)),[1,0,1,1,1,1,1,0]);
E.J106.set(jt,25,3);E.J106.set(jt,21,0);assert.equal(jt[17],0x07);assert.equal(jt[16],82|32);
assert.deepEqual(E.J106.bytes(E.J106.nibbles(jt)),jt);assert(E.J106.valid(E.J106.init()));
assert.deepEqual(E.J106.read(new Uint8Array([...jf,240,65,50,0,5,64,247,...jf])).voices.length,2);
const jfac=E.J106.read(new Uint8Array(readFileSync('assets/j106-factory/j106-factory.syx'))).voices;   /* Roland's factory bank imports */
assert.deepEqual([jfac.length,jfac[0].name,jfac[127].name,jfac[0].raw[16]],[128,'PATCH A11','PATCH B88',0x51]);
assert.throws(()=>E.J106.decode(jf.slice(0,-2).concat([247])));
/* HYDRA: the patch, its message, its fields against the firmware's generator and layout */
{
  const h=E.HY.init();assert(E.HY.valid(h));assert.equal(E.HY.name(h),'INIT HYDRA');assert.equal(h.length,144);
  const frame=E.HY.sysex(h);assert.equal(frame.length,294);assert.deepEqual(Array.from(frame.slice(0,5)),[240,0x7D,0x48,0x59,1]);assert.equal(frame[293],247);
  assert.deepEqual(Array.from(E.HY.decode(Array.from(frame))),Array.from(h));assert.deepEqual(Array.from(E.HY.bytes(E.HY.nibbles(h))),Array.from(h));
  assert.equal(E.HY.read(new Uint8Array([...frame,...frame])).voices.length,2);
  const waves=[...readFileSync('tools/gen_hy_waves.py','utf8').matchAll(/\("([A-Z0-9 ]+)", (?:spectrum|series|shape|vowel|formants|seeded)\(/g)].map(m=>m[1]);
  assert.equal(waves.length,56);assert.deepEqual(Array.from(E.HY.WAVES),waves);
  assert.equal(E.HY.get(h,11),0);E.HY.set(h,11,-12);assert.equal(h[11],24);assert.equal(E.HY.get(h,11),-12);assert(E.HY.valid(h));
  const over=h.slice();over[70]=11;assert(!E.HY.valid(over));
  const hp=E.cleanPatch({name:'H',engine:25,p:[],hy:h});assert.deepEqual(Array.from(E.libraryFile("bank",[hp],{}).patches[0].hy),Array.from(h));
  assert.throws(()=>E.cleanPatch({engine:19,p:[],hy:h}));
  const lib=E.readSysexLibrary(frame,'x.syx',{});assert.equal(lib.length,1);assert.equal(lib[0].engine,25);
}
const jp=E.cleanPatch({name:'J',engine:21,p:[],j106:jt});assert.deepEqual(E.libraryFile('bank',[jp],{}).patches[0].j106,jt);
assert.throws(()=>E.cleanPatch({engine:19,p:[],j106:jt}));
assert(!info.motionMax && !info.patterns && !info.chainRows);
const tabs=[...html.matchAll(/<button[^>]+data-tab="([^"]+)"/g)].map(m=>m[1]);
assert.deepEqual(tabs,['sound','fm6','library','settings']);
for(const id of ['p-sequencer','p-tracks','p-projects','trackbar','steptables'])assert(!html.includes(`id="${id}"`));
const ids=new Set([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]));
for(const m of html.matchAll(/\$\("([^"]+)"\)/g))assert(ids.has(m[1]),`Missing DOM element: ${m[1]}`);
assert(html.indexOf('id="backup-panel"')>html.indexOf('id="p-settings"'));
const sent=[];const libraryPeer=async([cmd,a])=>{sent.push([cmd,Array.from(a)]);return [a[0],a[1],a[2],0];};
await E.bank.store(libraryPeer,383,'LAST',info,0);await E.bank.load(libraryPeer,383,info,0);
assert.equal(sent[0][0],80);assert.deepEqual(sent[0][1].slice(0,4),[3,127,2,0]);assert.deepEqual(sent[1],[80,[4,127,2,0]]);
await E.bank.put(libraryPeer,5,{name:'PATCH A22',engine:21,p:Array(info.pcount).fill(0),j106:jt},info);
{const a=sent[2][1],tail=a.slice(a.length-36-3);assert.equal(a[3],21);assert.deepEqual(tail.slice(0,3),[4|32,31,0]);}
assert.throws(()=>E.cleanPatch({engine:10,p:[]}));
const clean=E.cleanPatch({engine:3,p:[],pattern:[[60,1]],grid:[[1,1]]});assert(!clean.pattern && !clean.grid);
const exported=E.libraryFile('bank',[clean],{}).patches[0];assert(!('pattern' in exported) && !('grid' in exported));
const file={format:'saadet-backup',version:2,objects:BACKUP_IDS.map(id=>{
 const bytes=readFileSync(`build/host/archive-${id}.bin`);return {id,size:bytes.length,crc:bkCrc(bytes),data:bytes.toString('base64')};
})};
readBackup(file);
const manifest=[1,0,BACKUP_IDS.length,...file.objects.flatMap(o=>[o.id,...bkU32(o.size),...bkU32(o.crc)])];
assert.equal(bkManifest(manifest).length,30);
const committed=[],writes=new Map(),staged=new Map();
async function peer([cmd,a]){
 if(cmd===65)return manifest;
 if(cmd===66){const id=a[0],off=bkR32(a,1),n=a[6]|a[7]<<7;const bytes=Buffer.from(file.objects.find(o=>o.id===id).data,'base64');return [id,0,...bkU32(off),n&127,n>>7,...bkPack(bytes.subarray(off,off+n))];}
 assert.equal(cmd,67);const [op,id]=a;assert(BACKUP_IDS.includes(id));
 if(op===0)staged.set(id,{size:bkR32(a,2),crc:bkR32(a,7),bytes:[]});
 if(op===1){const st=staged.get(id);assert.equal(bkR32(a,2),st.bytes.length);const n=Math.min(256,st.size-st.bytes.length);st.bytes.push(...bkUnpack(a.slice(7),n));}
 if(op===2){const st=staged.get(id);assert.equal(st.bytes.length,st.size);assert.equal(bkCrc(st.bytes),st.crc);committed.push(id);writes.set(id,Buffer.from(st.bytes));staged.delete(id);}
 if(op===3)staged.delete(id);
 return [op,id,0];
}
const captured=await captureBackup(peer,'test');assert.deepEqual(captured.objects,file.objects);
await restoreBackup(peer,file);assert.deepEqual(committed.slice(-2),[1,32]);assert.equal(committed.length,30);
for(const o of file.objects)assert.deepEqual(writes.get(o.id),Buffer.from(o.data,'base64'));
const reject=structuredClone(file);reject.objects[0].id=2;assert.throws(()=>readBackup(reject));
assert.throws(()=>readBackup({...file,format:'felucca-backup'}));
for(const id of [1,28,32]){
 const bad=structuredClone(file),o=bad.objects.find(o=>o.id===id);let b=Buffer.from(o.data,'base64');if(!b.length)b=Buffer.alloc(3716);b[0]^=1;o.size=b.length;o.crc=bkCrc(b);o.data=b.toString('base64');assert.throws(()=>readBackup(bad));
}
let commits=0;await assert.rejects(restoreBackup(async([cmd,a])=>cmd===65?manifest:(a[0]===2&&(commits++),[a[0],a[1],a[0]===0&&a[1]===32?2:0]),file));assert.equal(commits,0);
/* a backup from before the performances still reads; a performance bank must be one */
{const old={...file,version:1,objects:file.objects.filter(o=>o.id<=32)};assert.equal(readBackup(old).objects.length,24);
 const short=structuredClone(file);short.objects.pop();assert.throws(()=>readBackup(short));
 const bad=structuredClone(file),o=bad.objects.find(o=>o.id===33),b=Buffer.alloc(16);o.size=16;o.crc=bkCrc(b);o.data=b.toString('base64');assert.throws(()=>readBackup(bad));
 assert.equal(bkManifest([1,0,24,...file.objects.filter(o=>o.id<=32).flatMap(o=>[o.id,...bkU32(o.size),...bkU32(o.crc)])]).length,24);}
for(const n of [0,1,7,8,256]){const bytes=Uint8Array.from({length:n},(_,i)=>i*137);assert.deepEqual(bkUnpack(bkPack(bytes),n),bytes);}
assert.throws(()=>bkUnpack([128,0],1));assert.throws(()=>bkManifest(manifest.slice(0,-1)));
console.log('SAADET WEB: real INFO, four tabs, DOM references, 384 slots, archive round trip, preflight, corrupt/foreign rejection passed');
