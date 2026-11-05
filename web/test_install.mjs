// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// The web installer's transfer against the simulated FM-1 (web/install/fakefm1.js), with the page's timers
// throttled as a background tab's are: the install must keep its pace, an interrupted one must resume, and
// m-bootloader must follow the firmware. Also the installer's decisions (flow.js) and the vendored fm1-base files.
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {Updater} from './install/fm1ota.js';
import {SteadyUpdater, GAP_MS} from './install/transfer.js';
import {fakeAccess} from './install/fakefm1.js';
import {describeDevice, planFor, progressModel, afterFailure, releaseVersion, remaining} from './install/flow.js';

const realSet = globalThis.setTimeout, realClear = globalThis.clearTimeout;
/* the page's clock in a background tab: nothing sooner than THROTTLE ms. Waits of a second or more (fm1ota.js's
   pauses between steps, the device's restarts and its 5 s tries) run faster so the test stays short. */
const device = {set: (f, ms) => realSet(f, ms >= 1000 ? ms / 10 : ms), clear: (t) => realClear(t)};
const THROTTLE = 250;
function throttle() {
  globalThis.setTimeout = (f, ms = 0, ...a) => realSet(f, ms >= 1000 ? ms / 20 : Math.max(ms, THROTTLE), ...a);
  return () => { globalThis.setTimeout = realSet; };
}
const image = (n, seed) => { const b = new Uint8Array(n); for (let i = 0; i < n; i++) b[i] = (i * 31 + seed * 17 + (i >> 9)) & 0xFF; return b; };
const firmware = image(0x8000, 1), boot = image(0x1000, 2);
const PRODUCT = 'FM-1_9010000';

{ /* a full install from M-VAVE's firmware, in a throttled page: every answer leaves at once, timestamped */
  const {access, device: fm1} = fakeAccess({images: {firmware, boot}, identity: 'FM-1_016', after: PRODUCT, timers: device});
  const steps = [];
  const restore = throttle();
  const t0 = Date.now();
  const got = await new SteadyUpdater(access).install(firmware, PRODUCT, (k) => steps.push(k));
  const ms = Date.now() - t0;
  assert.equal(got, PRODUCT);
  const reads = fm1.sends.filter((s) => s.delay > 0);
  assert(reads.length > 100, `only ${reads.length} timestamped answers`);
  assert(reads.every((s) => s.delay > 0 && s.delay <= GAP_MS + 1), 'answers carry the 10 ms gap as a MIDI timestamp');
  assert(ms < reads.length * THROTTLE / 2, `throttled page took ${ms} ms for ${reads.length} reads`);
  for (const k of ['start', 'verify', 'loader', 'write', 'reboot', 'done']) assert(steps.includes(k), `step ${k}`);
  console.log(`install from stock: ${reads.length} reads in ${ms} ms with page timers throttled to ${THROTTLE} ms`);

  /* then m-bootloader, from the firmware just installed */
  const after = await new SteadyUpdater(access).installBoot(boot, PRODUCT, () => {});
  restore();
  assert.equal(after, PRODUCT);
  assert(fm1.log.includes('m-bootloader installed'), fm1.log.join('; '));
}

{ /* step 2's read loop alone, fm1-base's and this one, in the same throttled page */
  const loop = async (Kind) => {
    const {access} = fakeAccess({images: {firmware}, identity: PRODUCT, state: 'loader', stride: 4, timers: device});
    const up = new Kind(access);
    const ota = await up.find((id) => /^ota-/i.test(id.model));
    const restore = throttle();
    const t0 = Date.now();
    ota.link.send([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7]);
    const s = await up.serve(ota.link, firmware, 0xF0000000, 20000, () => {});
    const ms = Date.now() - t0;
    restore();
    ota.link.close();
    assert(s.finished);
    return {ms, served: s.served};
  };
  const upstream = await loop(Updater), steady = await loop(SteadyUpdater);
  assert.equal(upstream.served, steady.served);
  assert(upstream.ms >= upstream.served * THROTTLE * 0.9, `upstream loop took only ${upstream.ms} ms`);
  assert(steady.ms < steady.served * THROTTLE / 5, `steady loop took ${steady.ms} ms`);
  console.log(`step 2, ${steady.served} reads, page timers throttled to ${THROTTLE} ms: fm1-base ${upstream.ms} ms, steady ${steady.ms} ms`);
}

{ /* the cable lost halfway through writing: the device waits in update mode and the install resumes */
  const {access, device: fm1} = fakeAccess({images: {firmware}, identity: 'FM-1_9000900', after: PRODUCT, interruptAt: 0.5, timers: device});
  const up = new SteadyUpdater(access);
  let phase = 'start';
  const restore = throttle();
  const err = await up.install(firmware, PRODUCT, (k) => { if (k === 'loader') phase = 'staged'; }).then(() => null, (e) => e);
  assert(err && ['lost', 'stopped'].includes(err.code), String(err));
  assert.equal(afterFailure(err.code, phase), 'paused');
  await new Promise((r) => realSet(r, 400));
  assert.equal(fm1.state, 'loader');
  const ota = await up.waitFor((id) => /^ota-/i.test(id.model), 10000);
  assert(ota, 'the device is back in update mode');
  const s2 = await up.write(ota, firmware, () => {});
  restore();
  assert(s2.finished);
  console.log('interrupted install resumed in update mode');
}

{ /* the installer's decisions */
  assert.equal(releaseVersion('FM-1_9010000'), '1.0');
  assert.equal(releaseVersion('FM-1_9010203'), '1.2.3');
  assert.equal(releaseVersion('FM-1_900'), null);
  const d = (text, extra = {}) => describeDevice({text, model: text.split('_')[0], ...extra}, PRODUCT);
  assert.deepEqual(d('FM-1_016'), {kind: 'stock', version: 'V16'});
  assert.deepEqual(d(PRODUCT), {kind: 'this', version: '1.0'});
  assert.deepEqual(d('FM-1_9000900'), {kind: 'saadet', version: '0.9'});
  assert.equal(d('FM-1_900').kind, 'dev');
  assert.equal(d('ota-FM-1_015').kind, 'loader');
  assert.equal(d('FM1_OFFICIAL_OPEN', {open: true}).kind, 'open');
  assert.equal(d('FM-1_6000900').kind, 'other');
  assert.deepEqual(planFor('stock', false), {firmware: true, resume: false, boot: false});
  assert.deepEqual(planFor('this', true), {firmware: false, resume: false, boot: true});
  assert.deepEqual(planFor('loader', true), {firmware: true, resume: true, boot: true});
  assert.equal(planFor('open', false), null);
  assert.equal(afterFailure('stopped', 'start'), 'safe');
  assert.equal(afterFailure('noloader', 'staged'), 'paused');
  assert.equal(afterFailure('noreturn', 'staged'), 'restart');
  assert.equal(afterFailure('bootrefused', 'boot'), 'partial');
  const pm = progressModel(firmware);
  assert(pm.writing(pm.write) <= 99 && pm.verify(pm.check) < pm.writing(1));
  assert.equal(progressModel(firmware, true).check, 0);
  assert.equal(remaining(10, 1000, 5000), null);
  assert.equal(remaining(500, 1000, 60000), 60);
}

{ /* the page and its words: every element the script names exists, every text key is in both languages */
  const page = readFileSync('web/install/index.html', 'utf8'), js = readFileSync('web/install/installer.js', 'utf8');
  const ids = new Set([...page.matchAll(/\bid="([^"]+)"/g)].map((m) => m[1]));
  for (const m of js.matchAll(/\$\("([^"]+)"\)/g)) assert(ids.has(m[1]), `index.html has no #${m[1]}`);
  const en = js.split('\n  en: {')[1].split('\n  ja: {')[0], ja = js.split('\n  ja: {')[1].split('\n};')[0];
  const keys = (block) => new Set([...block.matchAll(/(?:^\s+|",\s+)(\w+):\s*"/gm)].map((m) => m[1]));
  const ke = keys(en), kj = keys(ja);
  assert.deepEqual([...ke].filter((k) => !kj.has(k)), [], 'English keys missing in Japanese');
  assert.deepEqual([...kj].filter((k) => !ke.has(k)), [], 'Japanese keys missing in English');
  const used = [...page.matchAll(/data-t="(\w+)"/g), ...js.matchAll(/\bt\("(\w+)"[,)]/g)].map((m) => m[1]);
  for (const k of used) assert(ke.has(k), `no text for ${k}`);
}

{ /* the vendored fm1-base files are the pinned revision's, unchanged */
  const rev = /REV="([0-9a-f]{12})/.exec(readFileSync('tools/build_base.py', 'utf8'))[1];
  const pinned = `build/deps/fm1-base-${rev}/web`;
  const sha = (p) => createHash('sha256').update(readFileSync(p)).digest('hex');
  if (existsSync(pinned)) {
    for (const f of ['fm1ota.js', 'fm1pkg.js', 'fm1ports.js']) assert.equal(sha(`web/install/${f}`), sha(`${pinned}/${f}`), `web/install/${f} differs from fm1-base ${rev}`);
  } else console.log(`vendored fm1-base files: skipped (no ${pinned}; run ./build.sh)`);
}

console.log('Installer transfer, resume, m-bootloader and decisions passed');
