// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// A simulated FM-1 behind a Web MIDI access object, for the installer's demo (?demo) and its tests. It speaks the
// update protocol as felucca-bsp's chip/ota.c and loader do: the identity on the handshake; on the upgrade
// message, read requests (cmd 0x30) answered within 3 tries of 5 s, then the "success" question; a restart (the
// port goes away, then comes back). Step 1 (the running firmware) reads a little and restarts into the update
// loader ("ota-FM-1_015"); step 2 (the loader) reads the image twice and restarts into the new firmware. Served
// m-bootloader's installer instead, step 1 restarts into the same firmware. The device tells the two packages
// apart by their first bytes, as the real one reads their headers. Nothing is written anywhere.
//
//   fakeAccess({ images, identity, after, state, interruptAt, stride, timers }) -> { access, device }
//     images       { firmware, boot }: the logical images the page serves (boot optional)
//     identity     what the running firmware reports ("FM-1_016": M-VAVE's own)
//     after        what it reports after a write (the package's identity)
//     state        "app" (default) or "loader" (an install was interrupted: the device waits in update mode)
//     interruptAt  0..1: step 2 loses the cable at that share of its reads, once (the device stays in update mode)
//     stride       reads only every stride-th block of a pass (a quicker demo; 1 = all)
//     timers       { set, clear }: the device's own clock (the tests throttle the page's, not the device's)

import { pack7, unpack7 } from "./fm1ota.js";

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const LOADER_ID = "ota-FM-1_015";
const BLOCK = 512, TRY_MS = 5000, TRIES = 3;
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

function frame(cmd, payload) {
  let s = 0;
  for (let i = 0; i < payload.length; i++) s += payload[i];
  return [0xF0, ...pack7([0x00, 0x59, cmd, payload.length & 0xFF, payload.length >> 8, 0, ...payload, ~s & 0xFF]), 0xF7];
}

function identityFrame(text) {
  const body = new Array(27).fill(0);
  for (let i = 0; i < text.length && i < 26; i++) body[i] = text.charCodeAt(i);
  return frame(0x11, body);
}

function readFrame(addr, len) {
  return frame(0x30, [0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0]);
}

// a reply to a read: its address and data, when it is one
function parseReply(pkt) {
  const u = unpack7(pkt.slice(1, -1));
  if (u.length < 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  const addr = (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len = u[11] | (u[12] << 8);
  return { addr, data: u.slice(14, 14 + len) };
}

class Port {
  constructor(device, name, type) {
    this.device = device; this.name = name; this.type = type; this.id = `${type}-${name}`;
    this.manufacturer = "Ellic Studio (simulated)"; this.connection = "closed"; this._handler = null;
  }
  get state() { return this.device.online ? "connected" : "disconnected"; }
  open() { this.connection = "open"; return Promise.resolve(this); }
  close() { this.connection = "closed"; return Promise.resolve(this); }
  set onmidimessage(f) { this._handler = f; }
  get onmidimessage() { return this._handler; }
  send(data, timestamp = 0) {
    if (!this.device.online) throw new DOMException("The port is disconnected", "InvalidStateError");
    const bytes = Array.from(data);
    const delay = Math.max(0, (timestamp || 0) - performance.now());
    this.device.sends.push({ bytes, delay });
    this.device.clock.set(() => this.device.receive(bytes), delay);
  }
}

class Device {
  constructor(o) {
    this.clock = o.timers || { set: (f, ms) => setTimeout(f, ms), clear: (t) => clearTimeout(t) };
    this.images = o.images || {};
    this.identity = o.identity || "FM-1_016";
    this.after = o.after || this.identity;
    this.state = o.state || "app";
    this.interruptAt = o.interruptAt || 0;
    this.stride = Math.max(1, o.stride || 1);
    this.online = true;
    this.sends = [];                          // what the page sent, with the delay it asked for
    this.log = [];                            // what the device did, in words
    this.reply = null;                        // the pending read's resolver
    this.busy = false;
    this.input = new Port(this, "Saadet FM-1", "input");
    this.output = new Port(this, "Saadet FM-1", "output");
  }

  emit(bytes) {
    const h = this.input._handler;
    if (h && this.online) h({ data: Uint8Array.from(bytes), timeStamp: performance.now() });
  }

  get text() { return this.state === "loader" ? LOADER_ID : this.identity; }

  receive(bytes) {
    if (!this.online) return;
    if (same(bytes, HS_QUERY)) { this.emit(identityFrame(this.text)); return; }
    if (same(bytes, UPGRADE)) { if (!this.busy) this.session(); return; }
    if (this.reply && bytes[0] === 0xF0) {
      const r = parseReply(bytes);
      if (r) { const f = this.reply; this.reply = null; f(r); }
    }
  }

  // one read: 3 tries of 5 s, as ota_read; -> its data, or null (the session gives up)
  async read(addr, len) {
    for (let t = 0; t < TRIES; t++) {
      if (!this.online) return null;
      const got = await new Promise((resolve) => {
        const tm = this.clock.set(() => { this.reply = null; resolve(null); }, TRY_MS);
        this.reply = (r) => { this.clock.clear(tm); resolve(r.addr === addr ? r.data : null); };
        this.emit(readFrame(addr, len));
      });
      if (got) return got;
    }
    return null;
  }

  restart(state, identity, ms = 1500) {
    this.online = false;
    this.state = state;
    if (identity) this.identity = identity;
    this.input.name = this.output.name = state === "loader" ? "FM-1" : "Saadet FM-1";
    return new Promise((r) => this.clock.set(() => { this.online = true; r(); }, ms));
  }

  async pass(from, to, size) {
    for (let a = from; a < to; a += BLOCK * this.stride) {
      const n = Math.min(BLOCK, size - a);
      if (n <= 0) break;
      if (!(await this.read(a, n))) return false;
      if (this.interruptAt && this.state === "loader" && a / to >= this.interruptAt) {
        this.interruptAt = 0;
        this.log.push("cable lost");
        await this.restart("loader", null, 2500);   // the update record stays: back in update mode
        return null;
      }
    }
    return true;
  }

  // the package served, known by its header: { size, boot }
  which(head) {
    const is = (img) => img && head.every((b, i) => img[i] === b);
    if (is(this.images.boot)) return { size: this.images.boot.length, boot: true };
    if (is(this.images.firmware)) return { size: this.images.firmware.length, boot: false };
    return null;
  }

  async session() {
    this.busy = true;
    try {
      const head = await this.read(0, 0x40);
      const pkg = head && this.which(head);
      if (!pkg) { this.log.push("refused: no answer or an unknown package"); return; }
      const { size, boot } = pkg;
      if (this.state === "app") {
        // step 1: the header and the loader (m-bootloader's installer: all of it), then the question
        if (!(await this.pass(0x400, boot ? size : Math.min(size, 0x6000), size))) { this.log.push("step 1 stopped"); return; }
        if (!(await this.finish(0xE0000000))) return;
        this.log.push(boot ? "m-bootloader staged" : "loader staged");
        if (boot) {
          await this.restart("app", null, 2500);     // m-bootloader's installer writes the boot area, then the firmware
          this.log.push("m-bootloader installed");
        } else {
          await this.restart("loader", null, 2000);
        }
      } else {
        // step 2: the whole image checked, then the app area written (each a pass of reads)
        const r1 = await this.pass(0, size, size);
        if (r1 === null) return;
        if (!r1) { this.log.push("step 2 stopped"); return; }
        const r2 = await this.pass(Math.min(0x4000, size), size, size);
        if (r2 === null) return;
        if (!r2) { this.log.push("step 2 stopped"); return; }
        if (!(await this.finish(0xF0000000))) return;
        this.log.push(`written: ${this.after}`);
        await this.restart("app", this.after, 2500);
      }
    } finally { this.busy = false; }
  }

  async finish(addr) {
    const ok = await this.read(addr, 8);
    if (!ok) this.log.push("no success answer");
    return !!ok;
  }
}

export function fakeAccess(options = {}) {
  const device = new Device(options);
  const access = {
    sysexEnabled: true,
    inputs: new Map([[device.input.id, device.input]]),
    outputs: new Map([[device.output.id, device.output]]),
    onstatechange: null,
    addEventListener() {}, removeEventListener() {},
  };
  return { access, device };
}
