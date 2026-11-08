// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// The web installer's transfer against the simulated FM-1 (web/install/fakefm1.js), with the page's timers
// throttled as a background tab's are: the install must keep its pace, an interrupted one must resume, and
// m-bootloader must follow the firmware; m-bootloader's MIDI mode, told by command 0x12, installs the package itself.
// Also the installer's decisions (flow.js) and the vendored fm1-base files.
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {Updater} from './install/fm1ota.js';
import {SteadyUpdater, GAP_MS} from './install/transfer.js';
import {fakeAccess} from './install/fakefm1.js';
import {PORTS, describeDevice, planFor, progressModel, afterFailure, releaseVersion, remaining} from './install/flow.js';
import {askInfo, isMbootId, parseInfo, olderBoot, bootRelease, flashChip} from './install/mboot.js';

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
const PRODUCT = 'FM-1_9010000', ELITE = 'SMK-37 Elite_910';
const PRODUCTS = {'FM-1': PRODUCT, 'SMK-37 Pro': 'SMK-37 Pro_910', 'SMK-37 Elite': ELITE, 'MKE-P37': 'MKE-P37_910'};

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

{ /* an SMK-37 Elite on M-VAVE's firmware: its port is none of fm1-base's usual names, so the page asks it by the
     SMK-37's; it installs the Elite's package through fm1-base's SMK loader and comes back running it */
  const {access} = fakeAccess({images: {firmware}, model: 'SMK-37 Elite', identity: 'SMK-37 Elite_016', after: ELITE, timers: device});
  assert.equal(await new SteadyUpdater(access).find(), null, 'without the SMK-37 names its port is never asked');
  const up = new SteadyUpdater(access);
  up.setNames(PORTS);
  const found = await up.find();
  assert.equal(found?.id.text, 'SMK-37 Elite_016');
  found.link.close();
  const wrong = await up.install(firmware, PRODUCT, () => {}).then(() => null, (e) => e);
  assert.equal(wrong?.code, 'model', 'the FM-1 package is refused on an SMK-37');
  const restore = throttle();
  const got = await up.install(firmware, ELITE, () => {});
  restore();
  assert.equal(got, ELITE);
  const back = await up.find();
  back.link.close();
  assert.deepEqual(describeDevice(back.id, PRODUCTS), {kind: 'this', model: 'SMK-37 Elite', version: '1.0'});
  console.log('SMK-37 Elite install from stock, found by its port name');
}

{ /* the model is the one the device answers, never its port's name: an SMK-37 Elite behind a name macOS cached for
     an FM-1 is an SMK-37 Elite, takes only the Elite's package, and comes back as one */
  const {access} = fakeAccess({images: {firmware}, model: 'SMK-37 Elite', identity: 'SMK-37 Elite_016', after: ELITE,
                               port: 'Saadet FM-1', timers: device});
  const up = new SteadyUpdater(access);
  up.setNames(PORTS);
  const found = await up.find();
  found.link.close();
  assert.equal(found.name, 'Saadet FM-1');
  assert.deepEqual(describeDevice(found.id, PRODUCTS), {kind: 'stock', model: 'SMK-37 Elite', version: 'V16'});
  assert.equal((await up.install(firmware, PRODUCT, () => {}).then(() => null, (e) => e))?.code, 'model');
  const restore = throttle();
  assert.equal(await up.install(firmware, ELITE, () => {}), ELITE);
  restore();
  console.log('model taken from the identity the device answers, not its cached port name');
}

{ /* m-bootloader's MIDI mode on an FM-1: the update loader's identity, told apart by its answer to 0x12, and the
     package installed through it in one throttled transfer (checked, then written), back as Saadet */
  const {access, device: fm1} = fakeAccess({images: {firmware}, state: 'mboot', identity: 'FM-1_016', after: PRODUCT, timers: device});
  const up = new SteadyUpdater(access);
  up.setNames(PORTS);
  const found = await up.find();
  assert.equal(found?.id.text, 'ota-FM-1_900');
  assert.equal(found.name, 'FM-1 Update');
  assert(isMbootId(found.id));
  const info = await askInfo(found.link);
  found.link.close();
  assert.deepEqual([info?.kind, info?.version, info?.jedec, info?.capacity], ['bootloader', '2.2-64f7c9c', 0x856014, 0x100000]);
  assert.equal(flashChip(info.jedec), 'Puya P25Q80H');
  const d = describeDevice(found.id, PRODUCTS, info);
  assert.deepEqual(d, {kind: 'mboot', version: '2.2-64f7c9c', stage: false, jedec: 0x856014, model: 'FM-1'});
  assert.deepEqual(planFor(d.kind, false), {firmware: true, resume: false, mboot: true, boot: false});
  const pm = progressModel(firmware, false, true), steps = [];
  let written = false, lastVerify = 0, firstWrite = null;
  const restore = throttle();
  const got = await up.mbootInstall(firmware, PRODUCT, PRODUCTS, pm, (k, n, b) => {
    steps.push(k);
    if (k === 'verify') lastVerify = b;
    if (k === 'write' && firstWrite === null) firstWrite = b;
  }, () => { written = true; });
  restore();
  assert.equal(got, PRODUCT);
  assert(written, 'written() heard once the transfer finished');
  for (const k of ['start', 'verify', 'write', 'reboot', 'done']) assert(steps.includes(k), `step ${k}`);
  assert(steps.indexOf('verify') < steps.indexOf('write') && lastVerify <= pm.check && firstWrite > 0 && firstWrite <= 512,
         'the check fills the prepare share, the write starts from its own zero');
  assert(fm1.log.includes(`written: ${PRODUCT}`) && fm1.state === 'app', fm1.log.join('; '));
  console.log('FM-1 in m-bootloader MIDI mode: found by 0x12, Saadet written through it');
}

{ /* an SMK-37 Elite in MIDI mode answers as a Pro: the page asks which it is, and installs only a model MIDI mode may
     be on; the FM-1 package is refused before anything is sent */
  const {access, device: smk} = fakeAccess({images: {firmware}, model: 'SMK-37 Elite', state: 'mboot', identity: 'SMK-37 Elite_016',
                                            after: ELITE, timers: device});
  const up = new SteadyUpdater(access);
  up.setNames(PORTS);
  const found = await up.find();
  assert.equal(found?.id.text, 'ota-SMK-37 Pro_900');
  const info = await askInfo(found.link);
  found.link.close();
  const d = describeDevice(found.id, PRODUCTS, info);
  assert.deepEqual([d.kind, d.model, d.choose], ['mboot', null, ['SMK-37 Pro', 'SMK-37 Elite']]);
  assert.equal(describeDevice(found.id, {'FM-1': PRODUCT}, info).kind, 'model', 'no SMK-37 package: nothing to install');
  const pm = progressModel(firmware, false, true);
  const before = smk.sends.length;
  const wrong = await up.mbootInstall(firmware, PRODUCT, PRODUCTS, pm, () => {}).then(() => null, (e) => e);
  assert.equal(wrong?.code, 'wrongloader');
  assert(!smk.sends.slice(before).some((x) => x.bytes.join() === '240,34,36,53,127,247'), 'no upgrade message sent');
  const restore = throttle();
  assert.equal(await up.mbootInstall(firmware, ELITE, PRODUCTS, pm, () => {}), ELITE);
  restore();
  console.log('SMK-37 Elite in MIDI mode: model chosen, Elite package written');
}

{ /* fm1-base's update loader shares MIDI mode's identity and stays silent on 0x12: it is a loader to resume, and
     mbootInstall refuses it before sending anything */
  const {access, device: smk} = fakeAccess({images: {firmware}, model: 'SMK-37 Pro', state: 'loader', identity: 'SMK-37 Pro_910', timers: device});
  const up = new SteadyUpdater(access);
  up.setNames(PORTS);
  const found = await up.find();
  assert.equal(found?.id.text, 'ota-SMK-37 Pro_900');
  assert(isMbootId(found.id));
  assert.equal(await askInfo(found.link, 300), null);
  found.link.close();
  assert.deepEqual(describeDevice(found.id, PRODUCTS, null), {kind: 'loader', model: 'SMK-37 Pro'});
  const err = await up.mbootInstall(firmware, 'SMK-37 Pro_910', PRODUCTS, progressModel(firmware, false, true), () => {})
    .then(() => null, (e) => e);
  assert.equal(err?.code, 'wrongloader');
  assert.equal(smk.state, 'loader');
  /* an FM-1 identity MIDI mode never gives is never asked 0x12 */
  assert(!isMbootId({text: 'ota-FM-1_015'}) && !isMbootId({text: 'ota-SMK-37 Elite_900'}) && !isMbootId({text: 'FM-1_900'}));
  console.log("fm1-base's loader: silent on 0x12, resumed as before");
}

{ /* the cable lost while MIDI mode writes: it stays in MIDI mode (and opens it by itself after a restart), so the page
     installs again from the start */
  const {access, device: fm1} = fakeAccess({images: {firmware}, state: 'mboot', after: PRODUCT, interruptAt: 0.7, timers: device});
  const up = new SteadyUpdater(access);
  const pm = progressModel(firmware, false, true);
  let phase = 'mboot';
  const restore = throttle();
  const err = await up.mbootInstall(firmware, PRODUCT, PRODUCTS, pm, () => {}, () => { phase = 'written'; }).then(() => null, (e) => e);
  assert(err && ['lost', 'stopped'].includes(err.code), String(err));
  assert.equal(afterFailure(err.code, phase), 'mboot');
  await new Promise((r) => realSet(r, 400));
  assert.equal(fm1.state, 'mboot');
  assert.equal(await up.mbootInstall(firmware, PRODUCT, PRODUCTS, pm, () => {}), PRODUCT);
  restore();
  console.log('interrupted MIDI mode install done again from MIDI mode');
}

{ /* mboot.js: only "MBOOT" answers count, versions compare by release */
  const {access} = fakeAccess({images: {firmware}, state: 'mboot', mboot: {version: '2.3-0000000', kind: 2, jedec: 0xB36014}, timers: device});
  const up = new SteadyUpdater(access);
  const found = await up.find();
  const info = await askInfo(found.link);
  found.link.close();
  assert.deepEqual([info.kind, info.version, flashChip(info.jedec)], ['stage', '2.3-0000000', 'UCUN UC25HQ80']);
  assert.equal(describeDevice(found.id, PRODUCTS, info).stage, true);
  assert.equal(parseInfo([0xF0, 0xF7]), null);
  assert.equal(parseInfo([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7]), null);
  assert.equal(flashChip(0x123456), '0x123456');
  assert.equal(bootRelease('2.2-64f7c9c'), '2.2');
  assert(olderBoot('2.0-c43a32f', '2.2') && olderBoot('1.3', '2.2') && olderBoot('2.2', '2.10'));
  assert(!olderBoot('2.2-64f7c9c', '2.2') && !olderBoot('2.3', '2.2') && !olderBoot('', '2.2') && !olderBoot('x', '2.2'));
  console.log('MIDI mode answers and versions');
}

{ /* the installer's decisions */
  assert.equal(releaseVersion('FM-1_9010000'), '1.0');
  assert.equal(releaseVersion('FM-1_9010203'), '1.2.3');
  assert.equal(releaseVersion('FM-1_900'), null);
  assert.equal(releaseVersion(ELITE, 'SMK-37 Elite_9', true), '1.0');
  assert.equal(releaseVersion('SMK-37 Elite_9010000', 'SMK-37 Elite_9', true), null);
  assert.equal(releaseVersion('FM-1_910'), null);
  const d = (text, extra = {}) => describeDevice({text, model: text.split('_')[0], ...extra}, PRODUCTS);
  assert.deepEqual(d('FM-1_016'), {kind: 'stock', model: 'FM-1', version: 'V16'});
  assert.deepEqual(d(PRODUCT), {kind: 'this', model: 'FM-1', version: '1.0'});
  assert.deepEqual(d('FM-1_9000900'), {kind: 'saadet', model: 'FM-1', version: '0.9'});
  assert.equal(d('FM-1_900').kind, 'dev');
  assert.deepEqual(d('ota-FM-1_015'), {kind: 'loader', model: 'FM-1'});
  assert.deepEqual(d('ota-FM1C_1a2b'), {kind: 'loader', model: 'FM-1'});
  assert.equal(d('FM1_OFFICIAL_OPEN', {open: true}).kind, 'open');
  assert.equal(d('FM-1_6000900').kind, 'other');
  /* the SMK-37s: each by its own model, in update mode only through fm1-base's loader */
  assert.deepEqual(d('SMK-37 Elite_016'), {kind: 'stock', model: 'SMK-37 Elite', version: 'V16'});
  assert.deepEqual(d('SMK-37 Pro_015'), {kind: 'stock', model: 'SMK-37 Pro', version: 'V15'});
  assert.deepEqual(d('MKE-P37_910'), {kind: 'this', model: 'MKE-P37', version: '1.0'});
  assert.deepEqual(d('SMK-37 Elite_909'), {kind: 'saadet', model: 'SMK-37 Elite', version: '0.9'});
  assert.equal(d('SMK-37 Elite_9010000').kind, 'other');
  assert.equal(d('SMK-37 Elite_900').kind, 'dev');
  assert.deepEqual(d('ota-SMK-37 Elite_900'), {kind: 'loader', model: 'SMK-37 Elite'});
  assert.deepEqual(d('ota-SMK-37 Elite_016'), {kind: 'vendor', model: 'SMK-37 Elite', text: 'ota-SMK-37 Elite_016'});
  assert.equal(d('SMK-37 Elite_91405').kind, 'other');
  assert.deepEqual(d('Keystep_100'), {kind: 'model', model: 'Keystep', text: 'Keystep_100'});
  assert.equal(describeDevice({text: 'SMK-37 Elite_016', model: 'SMK-37 Elite'}, {'FM-1': PRODUCT}).kind, 'model');
  assert.equal(planFor('vendor', false), null);
  assert.deepEqual(planFor('stock', false), {firmware: true, resume: false, boot: false});
  assert.deepEqual(planFor('this', true), {firmware: false, resume: false, boot: true});
  assert.deepEqual(planFor('loader', true), {firmware: true, resume: true, boot: true});
  assert.deepEqual(planFor('mboot', true), {firmware: true, resume: false, mboot: true, boot: true});
  assert.equal(planFor('open', false), null);
  assert.equal(afterFailure('stopped', 'start'), 'safe');
  assert.equal(afterFailure('noloader', 'staged'), 'paused');
  assert.equal(afterFailure('noreturn', 'staged'), 'restart');
  assert.equal(afterFailure('bootrefused', 'boot'), 'partial');
  assert.equal(afterFailure('stopped', 'mboot'), 'mboot');
  assert.equal(afterFailure('noreturn', 'written'), 'restart');
  assert.equal(afterFailure('mismatch', 'written'), 'restart');
  const pm = progressModel(firmware);
  assert(pm.writing(pm.write) <= 99 && pm.verify(pm.check) < pm.writing(1));
  assert.equal(progressModel(firmware, true).check, 0);
  const mb = progressModel(firmware, false, true);
  assert.deepEqual([mb.check, mb.write], [0x420 + firmware.length, firmware.length - 0x4000]);
  assert.equal(describeDevice({text: 'ota-FM-1_900', model: 'ota-FM-1'}, PRODUCTS, null).kind, 'loader');
  assert.equal(remaining(10, 1000, 5000), null);
  assert.equal(remaining(500, 1000, 60000), 60);
}

{ /* the page and its words: every element the script names exists, every text key exists, English only */
  const page = readFileSync('web/install/index.html', 'utf8'), js = readFileSync('web/install/installer.js', 'utf8');
  const ids = new Set([...page.matchAll(/\bid="([^"]+)"/g)].map((m) => m[1]));
  for (const m of js.matchAll(/\$\("([^"]+)"\)/g)) assert(ids.has(m[1]), `index.html has no #${m[1]}`);
  const en = js.split('\n  en: {')[1].split('\n};')[0];
  const keys = (block) => new Set([...block.matchAll(/(?:^\s+|",\s+)(\w+):\s*"/gm)].map((m) => m[1]));
  const ke = keys(en);
  assert(!/[\u3040-\u30ff\u4e00-\u9fff]/.test(page + js), 'the installer is English only');
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

console.log('Installer transfer, resume, m-bootloader, MIDI mode and decisions passed');
