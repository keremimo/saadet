// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Portions Copyright (C) 2026 Charles Vestal
//
// The installer's decisions, without the page: what a connected FM-1 or SMK-37 is running, what an install will do, how
// far it has got, and what a failure leaves behind. progressModel() follows fm1-base's installer.html.

import { ufwEntries } from "./fm1pkg.js";

export const IS_OTA = (id) => /^ota-/i.test(id.model);
// port names asked besides fm1-base's usual ones (fm1ota.js setNames): Saadet's own on the SMK-37 ("Saadet Elite
// SMK-37") and M-VAVE's SMK-37 firmware's ("SMK-37 Elite Midi", "MKE-P37 MIDI", "Starrykey")
export const PORTS = ["saadet", "smk", "elite", "mke-p37", "starry"];
const OPEN_RE = /^FM1_(OFFICIAL_OPEN|COMMUNITY_\d+)$/;

// the model an identity names, an update loader's without "ota-": "SMK-37 Elite_016" -> "SMK-37 Elite"
export const modelOf = (id) => id.model.replace(/^ota-/i, "");

// "FM-1_9010200" -> "1.2", "FM-1_9010203" -> "1.2.3" (fm1-base tools/build.py: the prefix, then XXYYZZ). An SMK-37's
// (short) ends in three digits at most, for compatibility: _9, then X and Y ("SMK-37 Elite_910" -> "1.0", for 1.0.x)
export function releaseVersion(product, prefix = "FM-1_9", short = false) {
  if (!product.startsWith(prefix)) return null;
  const m = (short ? /^(\d)(\d)()$/ : /^(\d\d)(\d\d)(\d\d)$/).exec(product.slice(prefix.length));
  if (!m) return null;
  const [x, y, z] = m.slice(1).map(Number);
  return z ? `${x}.${y}.${z}` : `${x}.${y}`;
}

// a connected device, from its identity and the packages this page has ({ model: product }): { kind, model, version }
//   loader   in update mode: an install waits to be finished (an FM-1's whatever its loader; an SMK-37's only in
//            fm1-base's own loader, "ota-SMK-37 Elite_900": its layout check is what keeps the package on its model)
//   vendor   an SMK-37 in M-VAVE's update mode: M-VAVE's updater finishes it, never this page
//   this     the version this page installs
//   saadet   another Saadet release; dev: a development build (MODEL_900)
//   stock    M-VAVE's own firmware (version: "V16"); open: M-VAVE's Open firmware (must go back to stock first)
//   other    another firmware for that model; model: a model this page has no package for
export function describeDevice(id, products) {
  if (!id) return { kind: "none" };
  const model = modelOf(id);
  if (IS_OTA(id)) {
    if (model === "FM-1" || !products[model]) return { kind: "loader", model: "FM-1" };
    return id.text === `ota-${model}_900` ? { kind: "loader", model } : { kind: "vendor", model, text: id.text };
  }
  if (id.open || OPEN_RE.test(id.text)) return { kind: "open", model: "FM-1" };
  const product = products[model];
  if (!product) return { kind: "model", model, text: id.text };
  const prefix = model + "_9", tail = id.text.slice(model.length);
  const short = model !== "FM-1";
  if (id.text === product) return { kind: "this", model, version: releaseVersion(product, prefix, short) };
  if (tail === "_900") return { kind: "dev", model };
  const v = releaseVersion(id.text, prefix, short);
  if (v) return { kind: "saadet", model, version: v };
  if (/^_0\d\d$/.test(tail) && tail !== "_099") return { kind: "stock", model, version: "V" + parseInt(tail.slice(1), 10) };
  return { kind: "other", model, text: id.text };
}

// what pressing Install does for a device: { firmware, resume, boot } or null (nothing to do)
export function planFor(kind, wantBoot) {
  if (kind === "loader") return { firmware: true, resume: true, boot: wantBoot };
  if (kind === "this") return wantBoot ? { firmware: false, resume: false, boot: true } : { firmware: true, resume: false, boot: false };
  if (kind === "open" || kind === "model" || kind === "vendor" || kind === "none") return null;
  return { firmware: true, resume: false, boot: wantBoot };
}

// the bytes the device reads, for the bar. Step 1 (the running firmware): the UFW header, the loader's head twice,
// its body. Step 2 (the loader): the header, the chip-key check, the whole flash.bin (checked before anything is
// erased), then the app area again as it writes it. A resume has step 2 only. Without a readable flash.bin entry
// the whole image stands for it.
export function progressModel(image, resume = false) {
  let ent = {};
  try { ent = ufwEntries(image); } catch (_) { /* not a UFW image */ }
  const fl = ent[0], ldr = ent[100];
  const flen = fl && fl.len > 0x4000 && fl.len <= 0x100000 ? fl.len : image.length;
  const check = resume ? 0 : 0x400 + 0x400 + (ldr && ldr.len > 0x20 && ldr.len < image.length ? ldr.len - 0x20 : 0);
  const write = 0x400 + 0x20 + flen + Math.max(0, flen - 0x4000);
  const total = check + write;
  return {
    check, write, total,
    verify: (b) => (Math.min(b, check) / total) * 100,
    writing: (b) => Math.min(99, ((check + Math.min(b, write)) / total) * 100),
  };
}

// minutes and seconds left at the rate so far, or null while it is too early to say
export function remaining(done, total, ms) {
  if (done < total * 0.04 || ms < 3000) return null;
  return Math.max(0, Math.round(((total - done) / done) * ms / 1000));
}

// what a failure leaves: "safe" (nothing written), "paused" (in update mode: resume), "restart" (written; the FM-1
// did not come back as expected), "partial" (the firmware installed, m-bootloader not)
export function afterFailure(code, phase) {
  if (phase === "boot") return code === "noreturn" ? "restart" : "partial";
  if (phase === "done") return "restart";
  if (phase === "staged") return code === "noreturn" || code === "mismatch" ? "restart" : "paused";
  return "safe";
}
