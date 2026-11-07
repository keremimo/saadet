// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Portions Copyright (C) 2026 Charles Vestal
// Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The installer page (index.html): find the FM-1, show what it runs, choose m-bootloader, then install in a
// screen of its own. Before writing: the rules (stay on the tab, keep the cable and power) and the backup. While
// writing: keepalive.js holds the page (one install at a time, awake, asks before leaving) and the screen says so
// again whenever the tab comes back. An install that stops after the FM-1 entered update mode can be resumed,
// now or on a later visit (a mark in localStorage and the device itself say so). fm1-base's installer.html is the
// origin of the m-bootloader option, the stock firmware path and the crash report.

import { productOf, logicalImage } from "./fm1pkg.js";
import { SteadyUpdater } from "./transfer.js";
import { hold, busyElsewhere } from "./keepalive.js";
import { IS_OTA, describeDevice, planFor, progressModel, remaining, afterFailure } from "./flow.js";

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const DEMO = params.has("demo") ? (params.get("demo") || "stock") : null;
const MARK = "saadet-install-pending";               // an install that has not reported Done (localStorage)
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_STOCK = (p) => /^FM-1_0\d\d$/.test(p) && p !== "FM-1_099";
const fail = (code, msg, detail = "") => Object.assign(new Error(msg), { code, detail });

const TEXT = {
  en: {
    navEditor: "Editor", navInstall: "Install", navManual: "Manual",
    demo: "Demo: a simulated FM-1. Nothing is written.", demoleave: "Leave the demo",
    kicker: "Firmware for the M-VAVE FM-1", title: "Install Saadet",
    lede: "Six engines on two parts: Prophet, FM6, CZ-1, JUNO-106, HYDRA and PPG, installed over USB from this page.",
    factBrowser: "Chrome or Edge", factCable: "A USB data cable", factTime: "About 3 minutes",
    resumeBannerH: "An installation didn't finish.",
    resumeBanner: "Your FM-1 is waiting in update mode. Connect it and press Find FM-1 to finish.",
    s1h: "Connect the FM-1",
    s1p: "Plug it straight into this computer, without a hub, and switch it on. Close other music apps and editor tabs. When the browser asks to use MIDI devices, choose Allow.",
    find: "Find FM-1", findAgain: "Look again", finding: "Looking for the FM-1…",
    s2h: "Recovery bootloader", optional: "Optional",
    mboot: "Also install m-bootloader {v}",
    mbootwhy: "A recovery menu (hold PLAY while switching on) and USB download mode (hold REC) that work even when a firmware won't start. It replaces M-VAVE's bootloader and stays through firmware updates.",
    mbootrisk: "It writes the FM-1's boot area, the one step where a power cut can need hardware recovery. Leave it off if you're unsure: you can add it later from this page.",
    mbootby: "By Julius Vitkauskas, MIT licence.", newtab: " (opens in a new tab)",
    s3h: "Install", s3p: "Back up your sounds first: Device, then Back up now, in Saadet Studio.",
    gohint: "Find your FM-1 first.",
    goInstall: "Install Saadet {v}", goUpdate: "Update to Saadet {v}", goReinstall: "Reinstall Saadet {v}",
    goResume: "Resume installation", goBoot: "Install m-bootloader",
    planInstall: "Saadet {v} replaces this FM-1's firmware. Back up your sounds first: Device, then Back up now, in Saadet Studio.",
    planUpdate: "Updates keep your saved sounds, but back them up first: Device, then Back up now, in Saadet Studio.",
    planResume: "The FM-1 is waiting in update mode. Resuming writes Saadet {v} again from the start of the write.",
    planThis: "Saadet {v} is already on this FM-1. Reinstalling is safe and keeps your sounds.",
    planBoot: "Saadet {v} is already on this FM-1, so only m-bootloader is written.",
    planOpen: "This FM-1 runs M-VAVE's Open firmware. Go back to M-VAVE's firmware first (below), then install Saadet.",
    planModel: "This is not an FM-1 ({id}). Saadet runs only on the M-VAVE FM-1.",
    devLoader: "In update mode: an installation is waiting to finish", devThis: "Saadet {v}", devSaadet: "Saadet {v}",
    devDev: "Saadet development build", devStock: "M-VAVE firmware {v}", devOpen: "M-VAVE Open firmware",
    devOther: "Another firmware ({id})", devModel: "Not an FM-1 ({id})",
    chipReady: "Ready", chipResume: "Update mode", chipCurrent: "Up to date", chipBlocked: "Can't install",
    nomidi: "This browser can't reach USB MIDI devices. Open this page in Chrome or Edge on a computer.",
    denied: "MIDI access was not allowed. Allow MIDI devices in the site settings (the icon in the address bar), then press Find FM-1 again.",
    notfound: "No FM-1 answered. Check that it is on and connected with a data cable (not a charging-only one), close other apps and tabs that use MIDI, then look again.",
    busy: "An installation is already running in another tab. Finish it there first.",
    loadfail: "The firmware package could not be loaded: {m}", badpkg: "The firmware package is damaged. Reload the page.",
    nopkg: "There is no firmware package beside this page. Try the demo instead:",
    pfh: "Before you start",
    pf1h: "Keep this tab open and in front", pf1: "Don't switch tabs, minimise the window or close it until the page says Done.",
    pf2h: "Leave the FM-1 connected and on", pf2: "Don't unplug the cable or switch the FM-1 off. Plug a laptop into power.",
    pf3h: "It takes about 3 minutes", pf3: "The FM-1's screen shows UPDATING, or stays dark. Both are normal.",
    pfboot: "m-bootloader is written last, after one more restart. Keep the power on until the page says Done.",
    pfbootonly: "Writing the boot area takes a few seconds. Keep the power on until the page says Done.",
    pfok: "I've backed up my sounds, or have nothing to keep.", cancel: "Cancel", start: "Start installation",
    runwarn: "Keep this tab open and in front. Don't unplug the FM-1.",
    runInstall: "Installing Saadet {v}", runResume: "Finishing the installation", runBoot: "Installing m-bootloader",
    runStock: "Installing M-VAVE's firmware",
    phPrepare: "Prepare", phWrite: "Write", phRestart: "Restart", phBoot: "m-bootloader",
    stStart: "Starting", stVerify: "Checking the package", stLoader: "Switching the FM-1 to update mode",
    stWrite: "Writing the firmware", stReboot: "Restarting the FM-1", stBootWait: "Waiting for Saadet to start",
    stBoot: "Writing m-bootloader. The FM-1 restarts by itself", stFind: "Finding the FM-1 in update mode",
    etaMin: "About {n} min left", etaSec: "Less than a minute left",
    stall: "Waiting for the FM-1… keep the cable connected.",
    backKept: "You left this tab for {s}. The installation kept going, but please stay here until it's done.",
    backSlow: "You left this tab for {s}, and the browser slowed the page. It continues now: please stay on this tab.",
    titleRun: "{p}% · Installing Saadet", titleHidden: "⚠ Come back: installing, {p}%", titleDone: "Done · Saadet",
    leave: "Installing. Leaving now can stop the installation.",
    endDoneH: "Saadet {v} is installed", endDoneBootH: "Saadet {v} and m-bootloader are installed",
    endDone: "The FM-1 has restarted with Saadet {v}. You can unplug it now.",
    endBootH: "m-bootloader is installed", endBoot: "The FM-1 has restarted with Saadet {v}.",
    endStockH: "M-VAVE's firmware is back", endStock: "The FM-1 has restarted with its original firmware.",
    tipEditor: "Open Saadet Studio to edit sounds and make backups.",
    tipBoot: "To check m-bootloader: switch the FM-1 off, hold PLAY and switch it on. Its recovery menu appears.",
    endPausedH: "Installation paused",
    endPaused: "The FM-1 is safe. It waits in update mode until the installation finishes, even if it's switched off.",
    tipCable: "Check the cable, or plug it in again. The screen shows UPDATING, or stays dark.",
    tipApps: "Close other music apps and editor tabs.", tipResume: "Then press Resume. The write starts again from the beginning.",
    endSafeH: "Nothing was changed", endSafe: "The FM-1 still runs its previous firmware.",
    endRestartH: "The FM-1 didn't come back",
    endRestart: "The write finished, but the FM-1 didn't report back. Switch it off and on again.",
    tipStill: "If it shows UPDATING or stays dark, it is still in update mode: press Find FM-1 and resume.",
    tipBrick: "If it doesn't start at all, see “If the FM-1 won't start” on this page.",
    endPartialH: "Saadet is installed, m-bootloader isn't",
    endPartial: "Saadet {v} is installed. m-bootloader could not be written and nothing else was changed. You can try again later from this page.",
    btnEditor: "Open Saadet Studio", btnManual: "Read the manual", btnClose: "Close", btnResume: "Resume installation",
    btnRetry: "Try again",
    e_notfound: "The FM-1 stopped answering before anything was written.",
    e_model: "The connected device is not an FM-1.",
    e_lost: "The USB connection was lost.", e_stopped: "The FM-1 stopped asking for data.",
    e_noloader: "The FM-1 in update mode was not found.", e_noreturn: "The FM-1 did not come back after restarting.",
    e_mismatch: "Written, but the FM-1 reports another version: {d}.", e_badreq: "The FM-1 asked for data outside the package.",
    e_open: "This FM-1 runs M-VAVE's Open firmware. Go back to M-VAVE's firmware first.",
    e_bootrefused: "The firmware refused m-bootloader.", e_other: "Error: {m}",
    mResumeH: "If an installation stopped",
    mResume1: "Nothing is lost. Until an installation finishes, the FM-1 starts in update mode: its screen shows UPDATING, or stays dark. That holds even after switching it off.",
    mResume2: "Connect it, open this page and press Find FM-1. The page offers to resume, and continues from the start of the write.",
    mStockH: "Go back to M-VAVE's firmware",
    mStock1: "This writes M-VAVE's own FM-1 firmware back over the same cable. It also puts back M-VAVE's bootloader, and what Saadet saved may be lost.",
    mStock2: "Download M-VAVE's firmware:", mStock3: "Choose the file you downloaded:",
    mStock4: "Connect the FM-1, press Find FM-1 above, then the button below.", mStockGo: "Install M-VAVE's firmware",
    stockok: "That is M-VAVE's FM-1 firmware ({p}).", stockbad: "That is not M-VAVE's FM-1 firmware. Choose the file from the link above.",
    stockfind: "Find the FM-1 above first.",
    mReportH: "Report a problem",
    mReport1: "If the FM-1 crashed, don't switch it off: the crash record survives a restart, not a power cycle. Connect it and read the record, then send it with what you were doing.",
    mReportGo: "Read the crash report", mReportCopy: "Copy", mReportLabel: "Crash report",
    reportwait: "Reading…", reportnone: "No answer. Check that the FM-1 is running Saadet.",
    reportok: "Read. Copy it and send it.", reportcopied: "Copied.",
    mBrickH: "If the FM-1 won't start",
    mBrick1: "With m-bootloader: switch off, hold PLAY and switch on for its recovery menu, or hold REC for USB download mode.",
    mBrick2: "Without it, the FM-1's USB recovery route can put a firmware back from a computer:",
    mBrickLink: "felucca-bsp recovery tools",
    credits: "Saadet by Kerem Kilic (Ellic Studio). Built on fm1-base by Charles Vestal and felucca-bsp, from Felucca by Leo Kuroshita (@kurogedelic), Hügelton Instruments. m-bootloader by Julius Vitkauskas.",
    legal: "Firmware: MPL-2.0. Editor: GPL-3.0-only. Third-party terms apply. Without warranty. M-VAVE and FM-1 are trademarks of their respective owners.",
    details: "Details", sec: "{n} s", min: "{n} min",
  },
};

const t = (k, vars = {}) => (TEXT.en[k] ?? k).replace(/\{(\w+)\}/g, (_, n) => vars[n] ?? "");

const st = {
  meta: null, image: null, boot: null, stock: null,  // the release, its images, a chosen stock package
  access: null, dev: null,                            // the MIDI access; the FM-1 found: { id, kind, version, text }
  running: false, run: null,                          // the install under way (its screen's state)
  connErr: null, stockErr: null, demo: null,
};
const ver = () => (st.meta ? st.meta.version : "");
const mark = (on) => { try { if (on) localStorage.setItem(MARK, st.meta.product); else localStorage.removeItem(MARK); } catch (_) { /* not kept */ } };

/* ---- language ---- */
function applyLang() {
  document.querySelectorAll("[data-t]").forEach((e) => { e.textContent = t(e.dataset.t, { v: st.meta?.boot?.version ?? "" }); });
  render();
}

/* ---- the release: release.json, its package and m-bootloader, each checked ---- */
const sha256 = async (raw) => [...new Uint8Array(await crypto.subtle.digest("SHA-256", raw))].map((b) => b.toString(16).padStart(2, "0")).join("");
async function fetchBytes(url) {
  const r = await fetch(url, { cache: "no-cache" });
  if (!r.ok) throw new Error(`${url}: HTTP ${r.status}`);
  return new Uint8Array(await r.arrayBuffer());
}
async function loadRelease() {
  let meta = null;
  try {
    const r = await fetch("release.json", { cache: "no-cache" });
    if (r.ok) meta = await r.json();
  } catch (_) { /* none here */ }
  if (DEMO) return demoRelease(meta);
  if (!meta) { pageFail(t("nopkg"), true); return; }
  st.meta = meta;
  try {
    const raw = await fetchBytes(meta.pkg);
    if ((await sha256(raw)) !== meta.sha256 || productOf(raw) !== meta.product) { pageFail(t("badpkg")); return; }
    st.image = logicalImage(raw);
  } catch (e) { pageFail(t("loadfail", { m: e.message })); return; }
  try {                                               /* offered only when it is exactly the pinned package */
    if (meta.boot) {
      const raw = await fetchBytes(meta.boot.pkg);
      if ((await sha256(raw)) === meta.boot.sha256 && productOf(raw) === "FM-1_099") st.boot = { image: logicalImage(raw), version: meta.boot.version };
    }
  } catch (_) { /* no option, then */ }
}

/* the demo: a simulated FM-1 (fakefm1.js) and small images of its own; nothing is fetched or written */
function demoImage(n, seed) { const b = new Uint8Array(n); for (let i = 0; i < n; i++) b[i] = (i * 131 + seed * 7 + (i >> 8)) & 0xFF; return b; }
function demoRelease(meta) {
  const version = meta?.version || "1.0";
  const product = meta?.product || "FM-1_9010000";
  st.meta = { name: "Saadet", version, product, boot: { version: meta?.boot?.version || "1.2" } };
  st.image = demoImage(0x30000, 1);
  st.boot = { image: demoImage(0x4800, 2), version: st.meta.boot.version };
  $("demo").hidden = false;
}
async function demoAccess() {
  if (DEMO === "none") return { inputs: new Map(), outputs: new Map(), sysexEnabled: true };
  if (!st.demo) {
    const { fakeAccess } = await import("./fakefm1.js");
    const p = st.meta.product;
    const opts = {
      stock: { identity: "FM-1_016" }, saadet: { identity: "FM-1_9000900" }, current: { identity: p },
      update: { identity: "FM-1_016", state: "loader" }, interrupt: { identity: "FM-1_016", interruptAt: 0.45 },
      open: { identity: "FM1_OFFICIAL_OPEN" },
    }[DEMO] || { identity: "FM-1_016" };
    st.demo = fakeAccess({ images: { firmware: st.image, boot: st.boot.image }, after: p, ...opts });
  }
  return st.demo.access;
}

function pageFail(text, demoLink = false) {
  $("pagefail").hidden = false;
  $("pagefailtext").textContent = text + " ";
  if (demoLink) {
    const a = document.createElement("a");
    a.href = "?demo";
    a.textContent = "?demo";
    $("pagefailtext").append(a);
  }
  $("find").disabled = true;
}

/* ---- finding the FM-1 ---- */
async function midi() {
  if (DEMO) return demoAccess();
  if (!navigator.requestMIDIAccess) throw fail("nomidi", "no Web MIDI");
  try { return await navigator.requestMIDIAccess({ sysex: true }); }
  catch (_) { throw fail("denied", "MIDI not allowed"); }
}

let finding = false;
async function connect() {
  if (finding || st.running) return;
  finding = true;
  st.connErr = null;
  render();
  try {
    st.access = await midi();
    if (await busyElsewhere()) throw fail("busy", "busy");
    const dev = await new SteadyUpdater(st.access).find();
    if (!dev) { st.dev = null; throw fail("notfound", "not found"); }
    dev.link.close();
    st.dev = { id: dev.id, ...describeDevice(dev.id, st.meta.product) };
  } catch (e) {
    st.connErr = ["nomidi", "denied", "notfound", "busy"].includes(e.code) ? e.code : "notfound";
  }
  finding = false;
  render();
}
$("find").addEventListener("click", connect);
$("mboot").addEventListener("change", render);

/* ---- the page's state, drawn ---- */
function render() {
  if (!st.meta) return;
  $("hver").textContent = ver();
  const dev = st.dev, plan = currentPlan();
  /* step 1 */
  const fb = $("find");
  fb.replaceChildren();
  if (finding) { const s = document.createElement("span"); s.className = "spin"; fb.append(s, t("finding")); }
  else fb.append(t(dev || st.connErr ? "findAgain" : "find"));
  fb.disabled = finding || st.running || !st.image;
  fb.classList.toggle("primary", !dev);
  $("connmsg").textContent = st.connErr ? t(st.connErr) : "";
  $("device").hidden = !dev;
  if (dev) {
    const words = { loader: "devLoader", this: "devThis", saadet: "devSaadet", dev: "devDev", stock: "devStock",
                    open: "devOpen", other: "devOther", model: "devModel" }[dev.kind];
    $("devwhat").textContent = t(words, { v: dev.version, id: dev.text || dev.id.text });
    const chip = dev.kind === "loader" ? ["chipResume", "warn"] : dev.kind === "this" ? ["chipCurrent", "ok"]
      : plan ? ["chipReady", "ok"] : ["chipBlocked", "bad"];
    $("devchip").textContent = t(chip[0]);
    $("devchip").className = "chip " + chip[1];
  }
  /* step 2: only with m-bootloader's package, never for M-VAVE's firmware */
  $("st-options").hidden = !st.boot;
  /* step 3 */
  const go = $("go");
  if (!dev || !plan) {
    go.textContent = t(dev?.kind === "loader" ? "goResume" : "goInstall", { v: ver() });
    go.disabled = true;
    $("gohint").hidden = !!dev;
    $("plantext").textContent = !dev ? t("s3p") : t(dev.kind === "open" ? "planOpen" : "planModel", { id: dev.id.text });
  } else {
    const label = plan.resume ? "goResume" : !plan.firmware ? "goBoot" : dev.kind === "this" ? "goReinstall"
      : ["saadet", "dev"].includes(dev.kind) ? "goUpdate" : "goInstall";
    go.textContent = t(label, { v: ver() });
    go.disabled = st.running;
    $("gohint").hidden = true;
    const words = plan.resume ? "planResume" : !plan.firmware ? "planBoot" : dev.kind === "this" ? "planThis"
      : ["saadet", "dev"].includes(dev.kind) ? "planUpdate" : "planInstall";
    $("plantext").textContent = t(words, { v: ver() });
  }
  $("st-connect").dataset.state = dev ? "done" : "active";
  $("st-options").dataset.state = dev && plan ? "active" : "todo";
  $("st-install").dataset.state = dev && plan ? "active" : "todo";
  $("stockgo").disabled = !st.stock || st.running;
  $("stockmsg").textContent = st.stockErr || (st.stock ? t("stockok", { p: st.stock.product }) : "");
  $("stockmsg").classList.toggle("msg", !!st.stockErr);
  /* an unfinished install: kept from an earlier visit, or the FM-1 itself in update mode */
  let pending = false;
  try { pending = !!localStorage.getItem(MARK); } catch (_) { /* unknown */ }
  if (pending && dev && dev.kind !== "loader" && !st.running) { mark(false); pending = false; }   /* it finished after all */
  $("resumebanner").hidden = !(pending || dev?.kind === "loader");
}

function currentPlan() {
  if (!st.dev) return null;
  return planFor(st.dev.kind, !!st.boot && $("mboot").checked);
}

/* ---- before writing: the rules, and the backup (asked every time) ---- */
function preflight(plan) {
  const d = $("preflight");
  $("pfboot").hidden = !plan.boot;
  $("pfboot").lastElementChild.textContent = t(plan.firmware ? "pfboot" : "pfbootonly");
  const askBackup = plan.firmware && !plan.resume;   /* in update mode there is no firmware running to back up */
  $("pfcheck").hidden = !askBackup;
  $("pfok").checked = false;
  $("pfgo").disabled = askBackup;
  d.showModal();
  (askBackup ? $("pfok") : $("pfgo")).focus();
  /* answered by the button itself (submit), not the dialog's close event, which a browser may hold back while
     the page is not drawn */
  return new Promise((resolve) => {
    const form = d.querySelector("form");
    const done = (go) => {
      form.removeEventListener("submit", submit);
      d.removeEventListener("cancel", cancel);
      d.close();
      resolve(go);
    };
    const submit = (e) => { e.preventDefault(); done(e.submitter?.value === "go"); };
    const cancel = (e) => { e.preventDefault(); done(false); };
    form.addEventListener("submit", submit);
    d.addEventListener("cancel", cancel);
  });
}
$("pfok").addEventListener("change", () => { $("pfgo").disabled = !$("pfok").checked; });

/* ---- the install's screen ---- */
const ICON = { done: "#i-check", paused: "#i-pause", safe: "#i-info", restart: "#i-alert", partial: "#i-alert" };
const FAVICON = (c) => `data:image/svg+xml,${encodeURIComponent(`<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'><rect width='16' height='16' rx='4' fill='${c}'/></svg>`)}`;
const baseTitle = document.title;

function log(line) {
  const el = $("log"), atEnd = el.scrollHeight - el.scrollTop - el.clientHeight < 24;
  el.append(line + "\n");
  if (atEnd) el.scrollTop = el.scrollHeight;
}

function phasesFor(plan) {
  return [plan.firmware && !plan.resume && "prepare", plan.firmware && "write", plan.firmware && "restart", plan.boot && "boot"].filter(Boolean);
}

function openRun(plan, isStock) {
  const r = { plan, isStock, pct: 0, phase: null, writeAt: 0, lastAt: Date.now(), hiddenPct: 0, list: phasesFor(plan) };
  st.run = r;
  const d = $("run");
  d.dataset.state = "running";
  $("end").hidden = true;
  $("log").textContent = "";
  $("backnote").hidden = $("stall").hidden = true;
  $("eta").textContent = "";
  $("runtitle").textContent = t(isStock ? "runStock" : plan.resume ? "runResume" : plan.firmware ? "runInstall" : "runBoot", { v: ver() });
  document.querySelectorAll("#phases li").forEach((li) => { li.hidden = !r.list.includes(li.dataset.ph); li.className = ""; });
  setPct(0);
  setStep(plan.resume ? "stFind" : "stStart", r.list[0]);
  $("favicon").href = FAVICON("#d08a00");
  if (!d.open) d.showModal();
}
$("run").addEventListener("cancel", (e) => { if (st.running) e.preventDefault(); });

function setPct(p) {
  const r = st.run;
  r.pct = Math.max(r.pct, Math.min(100, Math.floor(p)));
  $("pct").textContent = r.pct;
  $("barfill").style.width = r.pct + "%";
  $("bar").setAttribute("aria-valuenow", r.pct);
  document.title = document.visibilityState === "hidden" ? t("titleHidden", { p: r.pct }) : t("titleRun", { p: r.pct });
}

function setStep(key, phase) {
  const r = st.run;
  $("runstep").textContent = t(key);
  if (phase && phase !== r.phase) {
    r.phase = phase;
    const at = r.list.indexOf(phase);
    document.querySelectorAll("#phases li").forEach((li) => {
      const i = r.list.indexOf(li.dataset.ph);
      li.className = i < 0 ? "" : i < at ? "past" : i === at ? "now" : "";
    });
  }
  $("bar").classList.toggle("indet", phase === "boot" && !r.plan.firmware);
}

// fm1ota.js's steps -> the screen. The firmware fills the bar, or most of it when m-bootloader follows.
function stepper(pm) {
  const r = st.run, share = r.plan.boot ? (r.plan.firmware ? 0.94 : 0) : 1;
  return (k, a, b) => {
    log(`${k} ${a ?? ""}${b != null ? " " + b : ""}`);
    r.lastAt = Date.now();
    $("stall").hidden = true;
    if (k === "start") setStep("stStart", "prepare");
    else if (k === "verify") { setStep("stVerify", "prepare"); setPct(pm.verify(b) * share); }
    else if (k === "loader") setStep("stLoader", "prepare");
    else if (k === "write") {
      if (!r.writeAt) r.writeAt = Date.now() - 1;
      setStep("stWrite", "write");
      setPct(pm.writing(b) * share);
      const s = remaining(b, pm.write, Date.now() - r.writeAt);
      $("eta").textContent = s == null ? "" : s >= 60 ? t("etaMin", { n: Math.ceil(s / 60) }) : t("etaSec");
    } else if (k === "reboot") { $("eta").textContent = ""; setStep("stReboot", "restart"); setPct(99 * share); }
    else if (k === "done" && r.plan.boot) setStep("stBootWait", "boot");
    else if (k === "bootstart") { setStep("stBoot", "boot"); setPct(share ? 96 : 30); }
    else if (k === "bootwrite") { setStep("stBoot", "boot"); setPct(share ? 98 : 70); }
  };
}

// no progress for a while during a transfer: say what the page is waiting for (UI only)
setInterval(() => {
  const r = st.run;
  if (!st.running || !r || !["prepare", "write"].includes(r.phase)) return;
  $("stall").hidden = Date.now() - r.lastAt < 12000;
}, 1000);

function away(ms) {
  const s = Math.round(ms / 1000);
  return s < 60 ? t("sec", { n: s }) : t("min", { n: Math.round(s / 60) });
}
const onHidden = () => {
  const r = st.run;
  if (!r) return;
  r.hiddenPct = r.pct;
  document.title = t("titleHidden", { p: r.pct });
};
const onVisible = (ms) => {
  const r = st.run;
  if (!r || !st.running) return;
  document.title = t("titleRun", { p: r.pct });
  if (ms < 1500) return;
  const kept = r.pct > r.hiddenPct || r.phase === "restart" || r.phase === "boot";
  $("backtext").textContent = t(kept ? "backKept" : "backSlow", { s: away(ms) });
  $("backnote").hidden = false;
};

function endRun(state, err) {
  const r = st.run, plan = r.plan, v = { v: ver() };
  const d = $("run");
  d.dataset.state = state;
  $("end").hidden = false;
  $("endicon").setAttribute("href", ICON[state]);
  const tips = [], acts = [];
  const button = (key, primary, fn) => {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "btn" + (primary ? " primary" : "");
    b.textContent = t(key);
    b.addEventListener("click", fn);
    acts.push(b);
  };
  const link = (key, href, primary) => {
    const a = document.createElement("a");
    a.className = "btn" + (primary ? " primary" : "");
    a.href = href;
    a.textContent = t(key);
    a.style.textDecoration = "none";
    acts.push(a);
  };
  const reason = err ? (err.code && TEXT.en["e_" + err.code] ? t("e_" + err.code, { d: err.detail }) : t("e_other", { m: err.message })) : "";
  let title, text;
  if (state === "done") {
    if (r.isStock) { title = t("endStockH"); text = t("endStock"); }
    else if (!plan.firmware) { title = t("endBootH"); text = t("endBoot", v); tips.push(t("tipBoot")); }
    else {
      title = t(plan.boot ? "endDoneBootH" : "endDoneH", v);
      text = t("endDone", v);
      tips.push(t("tipEditor"));
      if (plan.boot) tips.push(t("tipBoot"));
    }
    if (!r.isStock) { link("btnEditor", "https://saadet.ellicstudio.com", true); link("btnManual", "../manual/index.html"); }
    button("btnClose", r.isStock, closeRun);
    setPct(100);
    document.title = t("titleDone");
    $("favicon").href = FAVICON("#1f8a46");
  } else if (state === "paused") {
    title = t("endPausedH"); text = t("endPaused");
    tips.push(reason, t("tipCable"), t("tipApps"), t("tipResume"));
    st.dev = { id: { text: "ota-FM-1", model: "ota-FM-1" }, kind: "loader" };
    button("btnResume", true, () => { closeRun(); start(); });
    button("btnClose", false, closeRun);
  } else if (state === "restart") {
    title = t("endRestartH"); text = err?.code === "mismatch" ? reason : t("endRestart");
    tips.push(t("tipStill"), t("tipBrick"));
    st.dev = null;
    button("btnClose", true, closeRun);
  } else if (state === "partial") {
    title = t("endPartialH"); text = t("endPartial", v);
    if (reason) tips.push(reason);
    button("btnClose", true, closeRun);
  } else {
    title = t("endSafeH"); text = t("endSafe");
    if (reason) tips.unshift(reason);
    tips.push(t("tipCable"), t("tipApps"));
    button("btnRetry", true, () => { closeRun(); connect(); });
    button("btnClose", false, closeRun);
  }
  $("endtitle").textContent = title;
  $("endtext").textContent = text;
  $("endtips").replaceChildren(...tips.filter(Boolean).map((x) => Object.assign(document.createElement("li"), { textContent: x })));
  $("endactions").replaceChildren(...acts);
  if (state !== "done") { document.title = baseTitle; $("favicon").href = FAVICON(state === "safe" ? "#777" : "#d08a00"); }
  (acts[0] || $("endtitle")).focus();
}

function closeRun() {
  $("run").close();
  document.title = baseTitle;
  render();
}

/* ---- the install ---- */
async function resumeWrite(up, image, product, step) {
  const ota = await up.waitFor(IS_OTA, 8000);
  if (!ota) throw fail("noloader", "the FM-1 in update mode was not found");
  const s2 = await up.write(ota, image, step);
  if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader stopped after ${s2.served} requests`);
  step("reboot");
  await sleep(3000);
  const back = await up.waitFor((id) => !IS_OTA(id), 40000);
  if (!back) throw fail("noreturn", "the FM-1 did not come back");
  back.link.close();
  if (back.id.text !== product) throw fail("mismatch", `the FM-1 reports ${back.id.text}`, back.id.text);
  step("done", back.id.text);
}
async function start(isStock = false) {
  if (st.running) return;
  const plan = isStock ? { firmware: true, resume: st.dev?.kind === "loader", boot: false } : currentPlan();
  if (!plan) return;
  st.running = true;                                  /* from the first click: a second one starts nothing */
  render();
  const release = (await preflight(plan)) && await hold({ leaveText: t("leave"), onHidden, onVisible });
  if (!release) {
    st.running = false;
    if (release === null) st.connErr = "busy";
    render();
    return;
  }
  render();
  openRun(plan, isStock);
  const image = isStock ? st.stock.image : st.image;
  const product = isStock ? st.stock.product : st.meta.product;
  const up = new SteadyUpdater(st.access, log);
  const step = stepper(progressModel(image, plan.resume));
  let phase = "start", state = "done", error = null;
  if (plan.firmware) mark(true);
  try {
    if (!st.access) st.access = await midi();
    if (plan.firmware) {
      if (plan.resume) { phase = "staged"; await resumeWrite(up, image, product, step); }
      else {
        await up.install(image, product, (k, a, b) => { if (k === "loader") phase = "staged"; step(k, a, b); },
                         isStock ? st.stock.sha256 : undefined);
      }
      phase = "done";
      mark(false);
    }
    if (plan.boot) { phase = "boot"; await up.installBoot(st.boot.image, product, step); }
  } catch (e) {
    if (isStock && e?.code === "mismatch") { mark(false); }   /* M-VAVE's firmware names itself its own way */
    else {
      state = afterFailure(e?.code, phase);
      error = e;
      if (state === "safe") mark(false);
      log(String(e?.stack || e));
    }
  }
  st.running = false;
  release();
  if (state === "done") st.dev = null;               /* what it runs now is shown on the next Find */
  endRun(state, error);
  render();
}
$("go").addEventListener("click", () => start(false));

/* ---- M-VAVE's firmware, chosen by the player (M-VAVE's server allows no cross-site download); the FM-1 checks
   the package itself before writing ---- */
$("stockfile").addEventListener("change", async (e) => {
  st.stock = null;
  st.stockErr = null;
  const f = e.target.files && e.target.files[0];
  if (f) {
    try {
      const raw = new Uint8Array(await f.arrayBuffer());
      const product = productOf(raw);
      if (!IS_STOCK(product)) st.stockErr = t("stockbad");
      else st.stock = { image: logicalImage(raw), product, sha256: await sha256(raw) };
    } catch (_) { st.stockErr = t("stockbad"); }
  }
  render();
});
$("stockgo").addEventListener("click", () => {
  if (!st.stock) return;
  if (!st.dev) { st.stockErr = t("stockfind"); render(); return; }
  start(true);
});

/* ---- the crash report: the firmware's log readout (F0 7D 4C 47 F7 asks; a frame a line, "end" last) ---- */
const LG = [0x7D, 0x4C, 0x47];
$("reportgo").addEventListener("click", async () => {
  const say = (k) => { $("reportstatus").textContent = t(k); };
  if (st.running) return;
  try { st.access = st.access || await midi(); } catch (e) { say(e.code); return; }
  $("reportgo").disabled = true;
  say("reportwait");
  const lines = [];
  try {
    const dev = await new SteadyUpdater(st.access).find((id) => !IS_OTA(id));
    if (dev) {
      lines.push(`${dev.id.text} (${dev.name}), ${new Date().toISOString()}`);
      dev.link.drain();
      dev.link.send([0xF0, ...LG, 0xF7]);
      const end = Date.now() + 4000;
      while (Date.now() < end) {
        const p = await dev.link.read(end - Date.now());
        if (!p) break;
        if (p.length < 5 || p[1] !== LG[0] || p[2] !== LG[1] || p[3] !== LG[2]) continue;
        const txt = String.fromCharCode(...p.slice(4, -1));
        if (txt === "end") break;
        lines.push(txt);
      }
      dev.link.close();
    }
  } catch (e) { lines.push(String(e?.message || e)); }
  $("reportgo").disabled = false;
  if (lines.length < 2) { say("reportnone"); return; }
  $("report").value = lines.join("\n");
  $("report").hidden = $("reportcopy").hidden = false;
  say("reportok");
});
$("reportcopy").addEventListener("click", async () => {
  try { await navigator.clipboard.writeText($("report").value); $("reportstatus").textContent = t("reportcopied"); }
  catch (_) { $("report").select(); }
});

/* ---- start ---- */
(async () => {
  applyLang();
  await loadRelease();
  applyLang();
  if (!st.image) return;
  /* MIDI already allowed (a returning visit, or a reload during an install): look without asking */
  let granted = !!DEMO && DEMO !== "none" && params.has("auto");
  try {
    if (!DEMO && navigator.permissions && navigator.requestMIDIAccess) {
      granted = (await navigator.permissions.query({ name: "midi", sysex: true })).state === "granted";
    }
  } catch (_) { /* unknown: wait for the button */ }
  if (granted) connect();
})();
