// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Real firmware capability/archive fixtures and an independent wire peer.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {BACKUP_IDS,bkManifest,bkU32,bkR32,bkPack,bkUnpack,bkCrc,readBackup,captureBackup,restoreBackup} from './fm1backup.js';
const html=readFileSync(new URL('editor.html',import.meta.url),'utf8');
const proto=html.split('/*PROTO-BEGIN*/')[1].split('/*PROTO-END*/')[0];
const E=vm.runInNewContext(`${proto};({parse,CMD,engineOrder,FM6,CZ,P5,J106,HY,HYS,bank,cleanPatch,libraryFile,readSysexLibrary})`,{setTimeout,clearTimeout,setInterval,clearInterval,console});
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
  /* the macros: names and destinations as hydra_patch.h has them; a slot's halves; a depth past 127 is no patch */
  const ph=readFileSync('firmware/src/hydra_patch.h','utf8');
  const names=[...ph.slice(ph.indexOf('#define HY_NAMES'),ph.indexOf('#define X(n) HN_')).matchAll(/X\(([A-Z]+)\)/g)].map(m=>m[1]);
  assert.deepEqual(Array.from(E.HY.MACRO_NAMES),names);
  const tg=[...ph.slice(ph.indexOf('#define HY_TARGETS'),ph.indexOf('#define X(n, b, l) HT_')).matchAll(/X\(([A-Z0-9]+), /g)].map(m=>m[1]);
  assert.equal(tg.length,32);assert.equal(E.HY.TARGETS.length,32);
  const m=E.HY.init();assert.equal(E.HY.get(m,300),1);assert.equal(E.HY.get(m,301),tg.indexOf('SCAN1'));assert.equal(E.HY.get(m,104),64);
  E.HY.set(m,302,8);E.HY.set(m,303,tg.indexOf('DRIVE'));assert.equal(m[105],7<<5|tg.indexOf('DRIVE'));E.HY.set(m,106,-127);assert.equal(m[106],0);assert(E.HY.valid(m));
  E.HY.set(m,303,0);assert.equal(m[105],0);assert.equal(m[106],127);
  const deep=m.slice();deep[104]=255;assert(!E.HY.valid(deep));
  /* ASM Hydrasynth patches: the message as Edisyn's notes give it (a dump request), then a patch of our own through its
     SysEx and through an ASM Manager bank (a stored ZIP), converted */
  assert.deepEqual(Array.from(E.HYS.message([4,0,0,0x7F])),[0xF0,0x00,0x20,0x2B,0x00,0x6F,0x47,0x64,0x74,0x6A,0x6B,0x51,0x51,0x41,0x41,0x48,0x38,0x3D,0xF7]);
  assert.deepEqual(Array.from(E.HYS.infoOf(E.HYS.message([4,0,0,0x7F]))),[4,0,0,0x7F]);
  const d=Array(2790).fill(0),w16=(o,v)=>{d[o]=v&255;d[o+1]=v>>8&255;};
  Array.from('Test Patch').forEach((c,i)=>{d[9+i]=c.charCodeAt(0);});
  d[80]=1;w16(82,4);d[84]=0xF4;d[86]=7;d[88]=100;w16(90,1024);w16(92,3);w16(94,2);   /* OSC 1: scan Saw, Sine (then OFF) */
  d[144]=2;w16(150,512);w16(152,256);w16(156,1024);                                       /* MUTANT 1: sync; MUTANT 2 dry */
  w16(274,1024);w16(276,512);d[308]=5;w16(310,512);w16(312,1024);w16(316,1024);w16(334,1024);w16(350,1024);
  w16(508,400);w16(510,512);d[30]=1;d[58]=1;d[52]=1;d[54]=40;                            /* ENV 2, MONO legato, glide */
  w16(1222,88);w16(1094,1024);w16(1606,512);Array.from('Brighten').forEach((c,i)=>{d[1630+i]=c.charCodeAt(0);});
  w16(1238,999);w16(1110,1024);Array.from('Wobble').forEach((c,i)=>{d[1647+i]=c.charCodeAt(0);});
  const syx=[...E.HYS.message([0x18,0]),...Array.from(E.HYS.chunks(d)).flatMap(x=>Array.from(x)),...E.HYS.message([0x1A,0])];
  const hv=E.HYS.read(Uint8Array.from(syx)).voices;assert.equal(hv.length,1);const hr=Array.from(hv[0].raw);
  assert(E.HY.valid(hv[0].raw));assert.equal(hv[0].name,'TEST PATCH');
  assert.deepEqual(hr.slice(0,14),[1,3,3,3,3,3,1,1,1,1,127,24,57,100]);
  assert.deepEqual(hr.slice(32,39),[3,0,0,64,32,127,0]);assert.deepEqual(hr.slice(59,61),[127,64]);
  assert.deepEqual(hr.slice(70,76),[4,127,127,0,0,126]);assert.equal(hr[81],127);assert.equal(hr[85],127);
  assert.deepEqual(hr.slice(103,107),[18,191,0,0]);assert.equal(hr[95],E.HY.MACRO_NAMES.indexOf('BRIGHT'));assert.equal(hr[96],0);
  assert.deepEqual({...hv[0].p},{ATK:0,DEC:70,SUS:64,REL:0,VCE:2,GLD:40});
  const zip=[],le=(v,n)=>Array.from({length:n},(_,i)=>v>>>(8*i)&255),entry=(name,body)=>{zip.push(...le(0x04034B50,4),10,0,0,8,0,0,0,0,0,0,...le(0,4),
    ...le(body.length,4),...le(body.length,4),...le(name.length,2),0,0,...Array.from(name,c=>c.charCodeAt(0)),...body);};
  entry('00Test Patch.patch',d.slice(4));entry('list.xml',Array.from('<list/>',c=>c.charCodeAt(0)));zip.push(...le(0x02014B50,4));
  assert.deepEqual(Array.from(E.HYS.read(Uint8Array.from(zip)).voices[0].raw),hr);
  const hl=E.readSysexLibrary(Uint8Array.from(syx),'h.syx',{keys:['LVL','ATK','DEC','SUS','REL','VCE','GLD']});
  assert.equal(hl[0].engine,25);assert.deepEqual(Array.from(hl[0].hy),hr);assert.deepEqual(Array.from(hl[0].p),[null,0,70,64,0,2,40]);
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
