// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Saadet archives use explicit object identities; object 32 is synth state.
export const BACKUP_IDS=[1,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,32];
const maxSize=()=>3840;
export const BACKUP_CMD = { LIST: 65, GET: 66, PUT: 67 };
const BACKUP_CHUNK = 256;
export const bkU32 = (n) => Array.from({ length: 5 }, (_, i) => (n >>> (i * 7)) & (i === 4 ? 15 : 127));
export const bkR32 = (a, off = 0) => {
  if (a.length < off + 5 || a[off + 4] > 15) throw new Error("Invalid archive number");
  return (a[off] | a[off + 1] << 7 | a[off + 2] << 14 | a[off + 3] << 21 | a[off + 4] << 28) >>> 0;
};
export function bkPack(bytes) {
  const a = [];
  for (let off = 0; off < bytes.length; off += 7) {
    const chunk = bytes.subarray(off, off + 7);
    a.push(chunk.reduce((m, b, i) => m | (b >>> 7) << i, 0), ...chunk.map((b) => b & 127));
  }
  return a;
}
export function bkUnpack(a, size) {
  const out = new Uint8Array(size); let i = 0, j = 0;
  while (j < size) {
    const n = Math.min(7, size - j), mask = a[i++];
    if (mask == null || mask >>> n) throw new Error("Invalid archive bytes");
    for (let k = 0; k < n; k++) {
      if (a[i] == null || a[i] > 127) throw new Error("Short archive chunk");
      out[j++] = a[i++] | (mask >>> k & 1) << 7;
    }
  }
  if (i !== a.length) throw new Error("Trailing archive bytes");
  return out;
}
export function bkCrc(bytes) {
  let c = 0xffffffff;
  for (const b of bytes) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xedb88320 : 0);
  }
  return (~c) >>> 0;
}
function bkCheck(rc) {
  if (rc) throw new Error(({ 1: "Invalid archive object", 2: "Archive data failed validation", 3: "Stop playback first", 4: "Flash write failed", 5: "Start a fresh backup" })[rc] || `Archive error ${rc}`);
}
export function bkManifest(a) {
  if(a[0]!==1)throw Error("Unsupported archive protocol");bkCheck(a[1]);
  if(a[2]!==BACKUP_IDS.length || a.length!==3+BACKUP_IDS.length*11)throw Error("Not a Saadet archive manifest");
  return BACKUP_IDS.map((id,i)=>{const at=3+i*11;if(a[at]!==id)throw Error("Unexpected archive object");const size=bkR32(a,at+1),crc=bkR32(a,at+6);if(size>maxSize(id)||(!size&&crc))throw Error("Invalid archive size");return {id,size,crc};});
}
function fnv(bytes){let h=2166136261;for(const b of bytes)h=Math.imul(h^b,16777619)>>>0;return h;}
export function readBackup(file) {
  if(typeof file==="string")file=JSON.parse(file);
  if(!file||file.format!=="saadet-backup"||file.version!==1||!Array.isArray(file.objects)||file.objects.length!==BACKUP_IDS.length)throw Error("Not a complete Saadet backup");
  const objects=file.objects.map((o,i)=>{
    if(!o||o.id!==BACKUP_IDS[i]||!Number.isInteger(o.size)||o.size<0||o.size>maxSize(o.id)||!Number.isInteger(o.crc)||o.crc<0||o.crc>0xffffffff||typeof o.data!=="string"||o.data.length!==4*Math.ceil(o.size/3)||!/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(o.data))throw Error("Invalid backup object");
    const bytes=Uint8Array.from(atob(o.data),c=>c.charCodeAt(0));if(bytes.length!==o.size||bkCrc(bytes)!==o.crc)throw Error("Backup checksum mismatch");
    const v=new DataView(bytes.buffer);
    if(o.id===32){if(o.size<512||v.getUint32(0,true)!==0x31534453||v.getUint32(4,true)!==o.size||v.getUint32(o.size-4,true)!==fnv(bytes.subarray(0,-4)))throw Error("Invalid synth state");}
    else if(o.id===1){if(o.size<604||v.getUint32(0,true)!==0x50455235)throw Error("Invalid settings record");}
    else if(o.size&&(o.size!==3716||v.getUint32(0,true)!==0x32425055||v.getUint16(4,true)!==206||v.getUint16(6,true)!==18))throw Error("Invalid shared sound bank");
    return {...o,bytes};
  });
  return {...file,objects};
}
const bkBase64=bytes=>{let s="";for(const b of bytes)s+=String.fromCharCode(b);return btoa(s);};
export async function captureBackup(request,firmware,onProgress=()=>{}) {
  const manifest=bkManifest(await request([BACKUP_CMD.LIST,[]],{timeout:3000,retries:0})),objects=[];
  const total=manifest.reduce((n,o)=>n+o.size,0);let done=0;
  for(const o of manifest){
    const bytes=new Uint8Array(o.size);
    for(let off=0;off<o.size;off+=BACKUP_CHUNK){const size=Math.min(BACKUP_CHUNK,o.size-off),a=await request([BACKUP_CMD.GET,[o.id,...bkU32(off),size&127,size>>>7]],{timeout:1000,retries:1});bkCheck(a[1]);if(a[0]!==o.id||bkR32(a,2)!==off||(a[7]|a[8]<<7)!==size)throw Error("Unexpected backup reply");bytes.set(bkUnpack(a.slice(9),size),off);done+=size;onProgress(done,total);}
    if(bkCrc(bytes)!==o.crc)throw Error("The device changed during backup; start again");objects.push({...o,data:bkBase64(bytes)});
  }
  const file={format:"saadet-backup",version:1,firmware,created:new Date().toISOString(),objects};readBackup(file);return file;
}
export async function restoreBackup(request,file,onProgress=()=>{}) {
  const archive=readBackup(file),ask=(r,o={})=>request(r,{timeout:4000,retries:0,...o}),put=async args=>{const a=await ask([BACKUP_CMD.PUT,args]);if(a[0]!==args[0]||a[1]!==args[1])throw Error("Unexpected restore reply");bkCheck(a[2]);return a;};
  bkManifest(await ask([BACKUP_CMD.LIST,[]]));
  // Preflight every object before the first committed write; begin/cancel only stages RAM.
  for(const o of archive.objects){await put([0,o.id,...bkU32(o.size),...bkU32(o.crc)]);await put([3,o.id]);}
  const total=archive.objects.reduce((n,o)=>n+o.size,0);let done=0;
  const order=[...archive.objects.filter(o=>o.id!==1&&o.id!==32),archive.objects.find(o=>o.id===1),archive.objects.find(o=>o.id===32)];
  for(const o of order){
    await put([0,o.id,...bkU32(o.size),...bkU32(o.crc)]);
    try{for(let off=0;off<o.size;off+=BACKUP_CHUNK){const chunk=o.bytes.subarray(off,off+BACKUP_CHUNK);await put([1,o.id,...bkU32(off),...bkPack(chunk)]);done+=chunk.length;onProgress(done,total);}await put([2,o.id]);}
    catch(error){try{await put([3,o.id]);}catch{}throw error;}
  }
}
export class BackupConnection {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d.at(-1) !== 247) return;
      if (this.pending?.cmd === d[4]) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(Array.from(d.slice(5, -1))); }
    };
  }
  async request([cmd, args], opt = {}) {
    if (this.pending || this.closed || this.input.state === "disconnected" || this.output.state === "disconnected") throw new Error("Backup connection closed or busy");
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending = null; reject(new Error("Backup timed out. Keep the FM-1 connected and stopped.")); }, opt.timeout || 1200);
      this.pending = { cmd, timer, resolve, reject };
      try { this.output.send([240, 125, 70, 76, cmd, ...args, 247]); }
      catch (e) { clearTimeout(timer); this.pending = null; reject(e); }
    });
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.reject(new Error("Backup connection closed")); }
  }
}
