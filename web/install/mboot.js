// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// m-bootloader's MIDI mode (github.com/jvitkauskas/m-bootloader, MIT: docs/SPEC.md §11.6, and its RAM stage §11.7),
// told apart the way M-Flasher's web page tells it (github.com/jvitkauskas/m-flasher, web/lib/protocol.js, MIT).
//
// MIDI mode answers the update protocol as an update loader does, under the same identity as fm1-base's loader
// ("ota-FM-1_900"; on every SMK-37, the Elite too, "ota-SMK-37 Pro_900"), and installs a package itself: it reads the
// package's header, checks the chip key, reads all of flash.bin and keeps each sector's CRC (nothing is erased so
// far), then reads each sector again and writes [0x4000, the end of flash.bin), 0x4000 last, skipping sectors the flash
// already holds, asks 0xF0000000 and restarts. The boot area, the key area and an SMK-37's calibration and Bluetooth
// sectors are never written. Its answer to command 0x12 ("MBOOT", the version, the flash) tells it from fm1-base's
// loader, which answers nothing.
//
// The same frame reaches M-VAVE's own update service, where command 0x21 erases a sector unchecked and the checksum
// does not cover the command byte. So this file sends 0x12 alone, with no body, and only to an identity MIDI mode
// gives (isMbootId); it never builds another command.

import { pack7, unpack7 } from "./fm1ota.js";

export const INFO = 0x12;
// the identities MIDI mode answers with: the FM-1's, and the one every SMK-37 shares (it can't tell a Pro from an Elite)
const MBOOT_IDS = ["ota-FM-1_900", "ota-SMK-37 Pro_900"];
export const isMbootId = (id) => !!id && MBOOT_IDS.includes(id.text);
// the SMK-37 models MIDI mode may be running on (m-bootloader's smk37 build is the Pro's and the Elite's)
export const MBOOT_SMK = ["SMK-37 Pro", "SMK-37 Elite"];

const INFO_FRAME = [0xF0, ...pack7([0x00, 0x59, INFO, 0, 0, 0, 0xFF]), 0xF7];   // no body: its checksum is ~0
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

// a frame -> { cmd, body } when it is one with a right length and checksum, else null
function parseFrame(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59) return null;
  const n = d[3] | (d[4] << 8) | (d[5] << 16);
  if (d.length !== n + 7) return null;
  let s = 0;
  for (let i = 6; i < 6 + n; i++) s += d[i];
  if ((~s & 0xFF) !== d[6 + n]) return null;
  return { cmd: d[2], body: d.slice(6, 6 + n) };
}

// the answer to 0x12 -> { kind: "bootloader" | "stage", version: "2.2-64f7c9c", protocol, key, capacity, jedec, uid },
// or null for anything else. Its body: "MBOOT", protocol, kind (1 the bootloader, 2 the RAM stage), flags, the chip key
// (u16), the capacity (u32), the JEDEC ID (u32), the flash's unique ID (8 bytes), two zeros, the version and a zero
export function parseInfo(pkt) {
  const f = parseFrame(pkt);
  if (!f || f.cmd !== INFO || f.body.length < 29) return null;
  const b = f.body;
  if (String.fromCharCode(...b.slice(0, 5)) !== "MBOOT" || (b[6] !== 1 && b[6] !== 2)) return null;
  let version = "";
  for (let i = 28; i < b.length && b[i]; i++) version += String.fromCharCode(b[i]);
  return {
    kind: b[6] === 2 ? "stage" : "bootloader", version, protocol: b[5], key: b[8] | (b[9] << 8),
    capacity: rd32(b, 10), jedec: rd32(b, 14), uid: b.slice(18, 26).map((x) => x.toString(16).padStart(2, "0")).join(""),
  };
}

// ask a device found with MIDI mode's identity what it is (a fm1ota.js link: drain, send, read) -> parseInfo's
// answer, or null: fm1-base's loader stays silent, m-bootloader before 2.2 refuses (command 0, body 01)
export async function askInfo(link, ms = 1000) {
  link.drain();
  if (!link.send(INFO_FRAME)) return null;
  const end = Date.now() + ms;
  while (Date.now() < end) {
    const p = await link.read(end - Date.now());
    if (!p) break;
    const f = parseFrame(p);
    if (f && f.cmd === 0) return null;
    const info = parseInfo(p);
    if (info) return info;
  }
  return null;
}

// "2.2-64f7c9c" -> "2.2"; is version a older than b ("2.0" < "2.2")
export const bootRelease = (version) => String(version || "").split("-")[0];
export function olderBoot(a, b) {
  const ra = bootRelease(a), rb = bootRelease(b), RELEASE = /^\d+(\.\d+)*$/;
  if (!RELEASE.test(ra) || !RELEASE.test(rb)) return false;          /* unknown: never offered as older */
  const x = ra.split(".").map(Number), y = rb.split(".").map(Number);
  for (let i = 0; i < Math.max(x.length, y.length); i++) {
    if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) < (y[i] || 0);
  }
  return false;
}

// the flash part a JEDEC ID names (the FM-1's two, and the SMK-37's third)
const FLASH = { 0x856014: "Puya P25Q80H", 0xB36014: "UCUN UC25HQ80", 0xEB6014: "0xEB6014" };
export const flashChip = (jedec) => FLASH[jedec] || "0x" + (jedec >>> 0).toString(16).toUpperCase().padStart(6, "0");
