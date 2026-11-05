/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Saadet's Bluetooth on the stack in platform/ble: part of device/bt_own.c, after bt_own_central.c
 * (tools/base_overlay.py). The rules are Melodee's (firmware/src/midi_ble.c), on the base's glue. One radio link, the
 * computer's or a device's:
 *   - the calibration: the stock firmware's stored trims when they are still there, kept in OBJ_BLE so they outlive
 *     that sector, else measured at every start (fm1_rf_cal.h);
 *   - the last device is looked for, not called blind: a round of SD_RELINK_SCAN_MS every SD_RELINK_MS, and a link only
 *     once it is heard, behind a private address too (its IRK). Between the rounds a computer can find Saadet, and with
 *     GLOBAL > BLE open there are no rounds;
 *   - never a new pairing loop: a lost link is looked for again, a refusal or a code nobody entered is not;
 *   - an attempt is bounded, longer while a code waits for its user. */
#define SD_RELINK_MS 30000u
#define SD_RELINK_SCAN_MS 10000u
#define SD_ATTEMPT_MS 10000u
#define SD_PAIRING_MS 45000u
#define SD_RETRY_SAVE_MS 30000u

static struct {
    struct ble_dev_store store;
    struct ble_found target;
    struct ble_keys keys;
    volatile uint8_t keys_new;
    uint8_t loaded, attempt, was_ready, was_pairing, again;
    uint8_t relink, relink_scan, page, hold;
    uint8_t dirty, save_failed;
    uint32_t deadline, relink_t, save_at;
    const char *message;
} sd_own;

static uint8_t sd_relink_scanning(void) { return sd_own.relink_scan; }

/* ---- the calibration */
static int sd_own_flash(void *ctx, uint32_t off, uint8_t *dst, uint32_t n)
{
    (void)ctx;                                    /* (read only: a stock VM candidate is never written) */
    return off > 0x100000u || n > 0x100000u - off ? -1 : st_read(off, dst, n);
}
static void sd_own_trims(struct ble_rf_trims *use)
{
    struct ble_vm_info info;
    struct ble_rf_trims vm;
    int save = 0;
    int complete = ble_vm_scan(sd_own_flash, 0, &info, &vm, 0, 0);
    if (ble_rf_choose(complete, &vm, info.area, own.sd_rf, use, &save) != BLE_RF_NONE) {
        fm1_rf_cal_on = 0;
        if (save)
            own_dirty = 1;
        return;
    }
    *use = OWN_TRIMS;                             /* 106, 107, 108: the SDK board's; 187 measured */
    fm1_rf_cal_on = 1;
}

/* ---- the last device */
static void sd_own_load(void)
{
    uint8_t raw[BLE_DEV_STORE_SIZE];
    if (sd_own.loaded)
        return;
    sd_own.loaded = 1;
    if (plat_store_load(OBJ_BT_LAST, raw, sizeof raw) == (int)sizeof raw)
        (void)ble_store_load(&sd_own.store, raw);
    else
        ble_store_reset(&sd_own.store);
    sd_own.relink = (uint8_t)ble_store_has_last(&sd_own.store);
    sd_own.relink_t = shim_ms() - SD_RELINK_MS;
}
static int sd_same_peer(const uint8_t addr[6], uint8_t rnd)
{
    const struct ble_dev *d = &sd_own.store.dev;
    return ble_store_has_last(&sd_own.store) &&
           ((((d->info & BLE_DEV_RANDOM) != 0) == rnd && ble_eq(addr, d->addr, 6)) ||
            (rnd && (d->info & BLE_DEV_IRK) && ble_rpa_resolve(d->irk, addr)));
}
static void sd_remember(void)
{
    if (!sd_same_peer(sd_own.target.addr, sd_own.target.addr_rand))
        ble_store_set_last(&sd_own.store, sd_own.target.addr, sd_own.target.addr_rand, sd_own.target.name, sd_own.target.kind);
}
static int sd_connect_target(void)
{
    struct ble_peer peer;
    const struct ble_dev *d = &sd_own.store.dev;
    int ok;
    ble_zero((uint8_t *)&peer, sizeof peer);
    ble_cpy(peer.addr, sd_own.target.addr, 6);
    peer.addr_rand = sd_own.target.addr_rand;
    if (sd_same_peer(peer.addr, peer.addr_rand)) {
        peer.bonded = (uint8_t)!!(d->info & BLE_DEV_BONDED);
        ble_cpy(peer.ltk, d->ltk, 16);
        ble_cpy(peer.rand, d->rand, 8);
        peer.ediv = ble_rd16(d->ediv);
        peer.sec = d->sec;                        /* (BLE_DEV_SEC_* are BLE_PEER_*) */
    }
    sd_own.keys_new = 0;
    own_held(1);
    ok = ble_central_connect(&peer);
    own_held(0);
    if (ok) {
        sd_own.attempt = 1;
        sd_own.was_ready = sd_own.was_pairing = 0;
        sd_own.deadline = shim_ms();
        shim_log("device: connecting");
    }
    return ok;
}
static void sd_scan(int on)
{
    own_held(1);
    ble_ll_scan(on);
    own_held(0);
}
/* the BLE interrupts' context: copy them (ble.h) */
static void sd_own_keys(const struct ble_keys *k)
{
    if (!sd_own.attempt)
        return;
    sd_own.keys = *k;
    RING_PUBLISH();
    sd_own.keys_new = 1;
}
static void sd_relink_poll(uint32_t now)
{
    uint8_t idx[BLE_SCAN_N];
    uint32_t i, n, t;
    if (!sd_own.relink || sd_own.attempt || sd_own.page || sd_own.hold || ble_off || ble_connected()) {
        if (sd_own.relink_scan && !sd_own.page)
            sd_scan(0);                           /* (the page's scan is the page's) */
        sd_own.relink_scan = 0;
        return;
    }
    t = now - sd_own.relink_t;
    if (t >= SD_RELINK_MS) {
        sd_own.relink_t = now;
        t = 0;
        sd_own.relink_scan = 1;
        sd_scan(1);
    }
    if (!sd_own.relink_scan)
        return;
    n = ble_scan_list(&own_scan, idx);
    for (i = 0; i < n; i++) {
        const struct ble_found *e = &own_scan.e[idx[i]];
        if (sd_same_peer(e->addr, e->addr_rand)) {
            sd_own.relink_scan = 0;
            sd_scan(0);
            sd_own.target = *e;
            sd_own.again = 0;
            (void)sd_connect_target();
            return;
        }
    }
    if (t >= SD_RELINK_SCAN_MS) {
        sd_own.relink_scan = 0;
        sd_scan(0);
    }
}
static void sd_save_poll(uint32_t now)
{
    uint8_t raw[BLE_DEV_STORE_SIZE];
    if (!sd_own.dirty || (sd_own.save_failed && now - sd_own.save_at < SD_RETRY_SAVE_MS) ||
        (plat_midi_count(PORT_BLE, 0) && plat_midi_ago(PORT_BLE, 0) < 2000u))   /* (a write holds the CPU: when MIDI pauses) */
        return;
    ble_store_save(&sd_own.store, raw);
    sd_own.save_at = now;
    sd_own.save_failed = (uint8_t)!!plat_store_save(OBJ_BT_LAST, raw, sizeof raw);
    if (!sd_own.save_failed)
        sd_own.dirty = 0;
}
/* the main loop, from own_central_poll */
static void sd_own_poll(void)
{
    uint32_t now = shim_ms();
    sd_own_load();
    if (ble_host_key_refused())
        sd_own.message = "FORGET ON PEER";        /* a central came with a key we do not know */
    if (sd_own.attempt) {
        uint8_t st = ble_central_state(), pairing = (uint8_t)ble_central_pairing();
        if (pairing != sd_own.was_pairing) {
            sd_own.was_pairing = pairing;
            sd_own.deadline = now;
        }
        if (sd_own.keys_new) {
            struct ble_keys k;
            own_held(1);
            k = sd_own.keys;
            sd_own.keys_new = 0;
            own_held(0);
            sd_remember();
            if (k.has & BLE_KEYS_LTK)
                ble_store_set_bond(&sd_own.store, k.ltk, k.rand, k.ediv, k.has & BLE_KEYS_AUTH);
            if (k.has & BLE_KEYS_ID)
                ble_store_set_id(&sd_own.store, k.irk, k.id, k.id_rand);
            sd_own.dirty = 1;
        }
        if (st == BLE_CS_READY && !sd_own.was_ready) {
            sd_own.was_ready = 1;
            sd_own.again = 0;
            sd_own.relink = 1;                    /* kept linked from now */
            sd_own.relink_t = now;
            sd_remember();
            sd_own.dirty = 1;
        } else if (st == BLE_CS_IDLE) {
            uint8_t why = ble_central_fail();
            sd_own.attempt = 0;
            if (why == BLE_CF_KEY) {
                ble_store_drop_bond(&sd_own.store);
                sd_own.dirty = 1;
            }
            if (why == BLE_CF_NEED_MITM && !sd_own.again) {   /* refused Just Works: one new link, the passkey (ble.h) */
                sd_own.again = 1;
                sd_remember();
                ble_store_drop_bond(&sd_own.store);
                ble_store_set_mitm(&sd_own.store);
                sd_own.dirty = 1;
                if (sd_connect_target()) {
                    sd_save_poll(now);
                    return;
                }
            }
            if (why != BLE_CF_LOST && why != BLE_CF_NONE)
                sd_own.relink = 0;                /* gone: looked for again; refused: not */
            if (why == BLE_CF_KEY)
                sd_own.message = "FORGET ON PEER";
            else if (why == BLE_CF_AUTH || why == BLE_CF_PAIRING || why == BLE_CF_NEED_MITM)
                sd_own.message = "PAIRING FAILED";
            else if (why == BLE_CF_NO_MIDI)
                sd_own.message = "NO MIDI SERVICE";
        } else if (!sd_own.was_ready && now - sd_own.deadline > (pairing ? SD_PAIRING_MS : SD_ATTEMPT_MS)) {
            if (pairing)
                sd_own.relink = 0;                /* a code nobody entered: no second prompt */
            own_held(1);
            ble_central_cancel();
            own_held(0);
            sd_own.attempt = 0;
            shim_log("device: no answer");
            if (!sd_own.relink)
                sd_own.message = "CONNECTION TIMEOUT";
        }
    }
    sd_relink_poll(now);
    sd_save_poll(now);
}

/* ---- plat.h: bt_own_central.c's, with the last device known to them */
void plat_ble_scan(int on)
{
    if (ble_state != BLE_READY)
        return;
    sd_own.page = (uint8_t)!!on;
    sd_own.relink_scan = 0;
    if (on)
        ble_scan_clear(&own_scan);
    sd_scan(on);
}
int plat_ble_connect(int i)
{
    uint8_t idx[BLE_SCAN_N];
    uint32_t n = ble_state == BLE_READY ? ble_scan_list(&own_scan, idx) : 0u;
    if (i < 0 || (uint32_t)i >= n || sd_own.attempt || ble_connected())
        return -1;
    sd_own.target = own_scan.e[idx[i]];
    sd_own.again = 0;
    return sd_connect_target() ? 0 : -1;
}

/* ---- what platform/ble_devices.h and ble_reconnect.h call */
void saadet_ble_relink(int on)
{
    sd_own_load();
    sd_own.relink = (uint8_t)(on && ble_store_has_last(&sd_own.store));
    sd_own.relink_t = shim_ms() - SD_RELINK_MS;
}
void saadet_ble_hold(int on) { sd_own.hold = (uint8_t)!!on; }
const char *saadet_ble_message(void)
{
    const char *m = sd_own.message;
    sd_own.message = 0;
    return m ? m : "";
}
const char *saadet_ble_device_name(void)
{
    return ble_central_state() != BLE_CS_IDLE ? sd_own.target.name : 0;
}
int saadet_ble_forget_device(void)
{
    uint8_t raw[BLE_DEV_STORE_SIZE];
    sd_own_load();
    ble_store_forget(&sd_own.store);
    sd_own.relink = sd_own.relink_scan = sd_own.dirty = 0;
    ble_store_save(&sd_own.store, raw);
    return !plat_store_save(OBJ_BT_LAST, raw, sizeof raw);
}
int32_t ble_midi_pairing_code(void) { return plat_ble_passkey(); }
void ble_midi_drop_host(void)
{
    own_held(1);
    if (own_role == 1u)
        ble_ll_disconnect(BLE_ERR_REMOTE_USER);
    own_held(0);
}
void ble_midi_reconnect(void)
{
    own_held(1);
    if (own_role == 1u) {
        ble_ll_disconnect(BLE_ERR_REMOTE_USER);   /* advertising again when it has gone */
    } else if (own_role == 0u) {
        ble_enable(0);
        ble_enable(1);
    }
    own_held(0);
}
int ble_midi_can_forget(void) { return !ble_connected() && ble_central_state() == BLE_CS_IDLE; }
/* our side of every bond: a new secret next time (ble_host_new_key), so the keys handed out are all gone with it */
int ble_midi_forget_all(void)
{
    if (!ble_midi_can_forget())
        return 0;
    own_held(1);
    ble_zero((uint8_t *)&bhs_key, sizeof bhs_key);
    ble_smp_reset();
    own_held(0);
    own.have_bond = 0;
    ble_zero(own.bond_rand, sizeof own.bond_rand);
    ble_zero(own.bond_ltk, sizeof own.bond_ltk);
    own.bond_ediv = 0;
    own_dirty = 0;
    return !plat_store_save(OBJ_BLE, &own, sizeof own);
}
