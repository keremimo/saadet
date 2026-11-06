/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The editor bridge: the synth build's SysEx on the host, so an editor's session code can be tested against the real
 * firmware/src/editor.c. tests/run_tests.sh builds it into build/host/editor_bridge. It boots as main.c does, on an
 * erased flash and plugged into USB, runs its clock to 1000 ms, then reads stdin a line at a time:
 *
 *   F0 7D 46 4C 01 F7
 *       A SysEx message: hex bytes from F0 to F7, two digits each, either case, a space or tab between them; the bytes
 *       between are data (below 80). They reach usb.c as USB-MIDI packets (midi_in_event, as an EP1 OUT packet brings
 *       them), so the editor's messages, the native synths', a DX7's or a CZ's all take the device's own path. The main
 *       loop's SysEx services then run in main.c's order: cz_service, fm6_service, ed_service (without the ed_sync it
 *       starts with: pushes go out on #sync alone), and a message none of them took is dropped, as ota_service drops it.
 *   #sync
 *       One ed_sync() pass: while WATCHed, CHANGED / TRACK_CHANGED / RELOAD for what changed on the device. Its own
 *       pacing holds: a pass runs at most every 5 ms, a value goes out at most every 20 ms, four frames at most a pass,
 *       and WATCH ends 3 s after the editor's last message. So put a #tick of 20 or more before a #sync that must see
 *       a change.
 *   #tick <ms>
 *       The clock on by ms (1 to 600000): the audio blocks of that time (MIDI in drained, an engine change completed),
 *       and every 16 ms the main loop's pass without ed_sync: the SysEx services above, then the panel (ui_input,
 *       settings_poll, ui_leds, ui_draw).
 *   #field <part> <kind> <id> <delta>
 *       A knob's turn of one panel field, through ui_field.c field_step (the path every knob and fader takes).
 *       part: 0 (A) or 1 (B), selected first when it is not (track_select, as the panel's PART does: a RELOAD at the
 *       next #sync; with one part, B is an error). kind: the field's FK_ kind by name (track, global, p5, fm6op, fm6v, fm6fn, cz, dev, lib, ble, j106,
 *       hy, hylab) or by number; fm6op.N names operator N (0 = OP1). id: the field's id within its kind (track: a P_ id,
 *       global: a G_ id, hy: a patch byte, j106: a JUNO field, ...). delta: detents, + clockwise, as field_step takes
 *       them (an enum or a short range moves one step; ui.c's fast-turn multiplier is not applied). "#error no field"
 *       when the sound has no such field.
 *   #note <on|off> <ch> <note> <vel>
 *       A note on or off as a USB-MIDI packet (cable 0) into midi_in_event: ch 1 to 16, note and vel 0 to 127. The
 *       audio drains it: it plays from the next #tick.
 *
 * Blank lines are skipped. For every other line stdout carries each SysEx message the firmware sent while doing it
 * (replies, pushes, native dumps), one a line in the input's form (upper-case hex, F0 to F7), then "#ok", or
 * "#error <why>" when the line could not be read or done. A message the firmware leaves open (the CZ handshake) prints
 * when its F7 goes out. Nothing else is written to stdout. */
#include <assert.h>
#include <ctype.h>
#define MELODEE_FLASH 1
#define MELODEE_USB_AUDIO 0
#define MELODEE_OTA 1                                    /* usb.c's SysEx frame and send ring, the native senders */
#include "synth_harness.c"
#define ED_BK_FLASH_PTR(off) (nor + (off))
static void bridge_drain(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { bridge_drain(); }           /* (the send ring full: the host reads it, as EP1 IN would) */
#include "../firmware/src/editor.c"

/* ------------------------------------------------------------ out --- */
static uint8_t out_msg[16384];                           /* (an FM6 bank dump: 4104 bytes) */
static uint32_t out_n, out_lost;
static void bridge_drain(void)                           /* the send ring as the host's USB-MIDI driver reads it */
{
    while (so_r != so_w) {
        uint32_t pkt = sx_out_q[so_r % SXQ], cin = pkt & 15u, k, n = cin == 5u ? 1u : cin == 6u ? 2u : 3u;
        so_r++;
        if (cin < 4u || cin > 7u)
            continue;                                    /* (this ring carries SysEx only) */
        for (k = 0; k < n; k++) {
            uint8_t b = (uint8_t)(pkt >> (8u * (k + 1u)));
            if (out_n < sizeof out_msg)
                out_msg[out_n++] = b;
            else
                out_lost = 1;
            if (b != 0xF7u)
                continue;
            for (uint32_t i = 0; i < out_n; i++)
                printf(i ? " %02X" : "%02X", out_msg[i]);
            putchar('\n');
            if (out_lost)
                fputs("editor_bridge: a message longer than its buffer was cut\n", stderr);
            out_n = out_lost = 0;
        }
    }
}

/* ------------------------------------------------------------ in --- */
static void usb_sysex(const uint8_t *m, uint32_t n)      /* a host's USB-MIDI driver: CIN 4 x 3 bytes, then 5 / 6 / 7 */
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t k = n - i > 3u ? 3u : n - i, pkt = i + k < n ? 4u : 4u + k;
        for (uint32_t j = 0; j < k; j++)
            pkt |= (uint32_t)m[i + j] << (8u * (j + 1u));
        midi_in_event(pkt);
        i += k;
    }
}
static void sysex_services(void)                          /* main.c's order; ed_sync waits for #sync */
{
    uint32_t run = ed_w.run_ms;
    cz_service();
    fm6_service();
    ed_w.run_ms = fm1_ms;                                /* (so the ed_sync ed_service starts with returns at once) */
    ed_service();
    ed_w.run_ms = run;
    if (sx_ready)
        ota_frame_done();                                /* ota_service: a message nobody took */
}
static uint32_t audio_due, loop_at;
static void bridge_tick(uint32_t ms)
{
    while (ms--) {
        fm1_ms++;
        host_ticks += 1000u * FM1_TICKS_PER_US;
        for (audio_due += FS; audio_due >= CTL * 1000u; audio_due -= CTL * 1000u) {
            int32_t out[CTL * 2];
            mix_block(out, CTL);
        }
        if (fm1_ms - loop_at >= 16u) {                   /* (main.c: ~60 passes a second) */
            loop_at = fm1_ms;
            sysex_services();
            ui_input();
            settings_poll();
            ui_leds();
            ui_draw();
        }
    }
}

/* ------------------------------------------------------------ lines --- */
static int num(const char *s, long lo, long hi, long *v)
{
    char *e;
    long x;
    if (!*s)
        return 0;
    x = strtol(s, &e, 10);
    if (*e || x < lo || x > hi)
        return 0;
    *v = x;
    return 1;
}
static uint32_t words(char *s, char **w, uint32_t max)    /* splits s in place; max + 1: too many */
{
    uint32_t n = 0;
    for (;;) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s)
            return n;
        if (n == max)
            return max + 1u;
        w[n++] = s;
        while (*s && *s != ' ' && *s != '\t')
            s++;
        if (*s)
            *s++ = 0;
    }
}
static int hexd(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static const char *line_sysex(char *s)
{
    static uint8_t m[sizeof out_msg];
    uint32_t n = 0;
    for (;;) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s)
            break;
        if (hexd(s[0]) < 0 || hexd(s[1]) < 0 || (s[2] && s[2] != ' ' && s[2] != '\t'))
            return "not a hex byte";
        if (n == sizeof m)
            return "message too long";
        m[n++] = (uint8_t)(hexd(s[0]) << 4 | hexd(s[1]));
        s += 2;
    }
    if (n < 2u || m[0] != 0xF0u || m[n - 1u] != 0xF7u)
        return "a message runs from F0 to F7";
    for (uint32_t i = 1; i + 1u < n; i++)
        if (m[i] & 0x80u)
            return "a data byte above 7F";
    usb_sysex(m, n);
    sysex_services();
    return 0;
}
static const char *const FK_NAMES[] = {"none", "track", "global", "p5", "fm6op", "fm6v", "fm6fn", "cz", "dev", "lib", "ble",
                                       "j106", "hy", "hylab"};
_Static_assert(sizeof FK_NAMES / sizeof *FK_NAMES == FK_HYLAB + 1u, "a name for every field kind (ui_field.c)");
static const char *line_field(char **w, uint32_t n)
{
    long part, kind = -1, sub = 0, id, delta;
    char *dot;
    uint32_t k;
    if (n != 4u)
        return "usage: #field <part> <kind> <id> <delta>";
    if (!num(w[0], 0, NTRK - 1, &part))
        return "no such part";
    if ((dot = strchr(w[1], '.'))) {
        *dot = 0;
        if (!num(dot + 1, 0, 5, &sub))
            return "no such operator";
    }
    for (k = 0; k < sizeof FK_NAMES / sizeof *FK_NAMES; k++)
        if (!strcmp(w[1], FK_NAMES[k]))
            kind = (long)k;
    if (kind < 0 && !num(w[1], 0, FK_HYLAB, &kind))
        return "no such field kind";
    if (dot && kind != FK_FM6OP)
        return "only fm6op takes an operator";
    if (!num(w[2], 0, 255, &id))
        return "no such field id";
    if (!num(w[3], -100000, 100000, &delta))
        return "no such delta";
    if ((uint32_t)part != song.sel)
        track_select((uint32_t)part);
    if ((uint32_t)part != song.sel)
        return "part not selectable (one part: GLOBAL > PARTS)";
    ui_field_t f = {(uint8_t)kind, (uint8_t)id, 0, (uint8_t)sub};
    if (!field_desc(&f))
        return "no field";
    field_step(&f, (int32_t)delta);
    return 0;
}
static const char *line_note(char **w, uint32_t n)
{
    long ch, note, vel;
    uint32_t on, st;
    if (n != 4u || (strcmp(w[0], "on") && strcmp(w[0], "off")))
        return "usage: #note <on|off> <ch> <note> <vel>";
    if (!num(w[1], 1, 16, &ch) || !num(w[2], 0, 127, &note) || !num(w[3], 0, 127, &vel))
        return "ch 1 to 16, note and vel 0 to 127";
    on = !strcmp(w[0], "on");
    st = (on ? 0x90u : 0x80u) | (uint32_t)(ch - 1);
    midi_in_event(st >> 4 | st << 8 | (uint32_t)note << 16 | (uint32_t)vel << 24);
    return 0;
}
static const char *line_control(char *s)
{
    char *w[8];
    long ms;
    uint32_t n = words(s, w, 8);
    if (n > 8u)
        return "too many words";
    if (!strcmp(w[0], "#sync")) {
        if (n != 1u)
            return "usage: #sync";
        ed_sync();
        return 0;
    }
    if (!strcmp(w[0], "#tick")) {
        if (n != 2u || !num(w[1], 1, 600000, &ms))
            return "usage: #tick <ms>, 1 to 600000";
        bridge_tick((uint32_t)ms);
        return 0;
    }
    if (!strcmp(w[0], "#field"))
        return line_field(w + 1, n - 1u);
    if (!strcmp(w[0], "#note"))
        return line_note(w + 1, n - 1u);
    return "unknown control line";
}

static void bridge_boot(void)                            /* main.c fm1_main's order, on an erased flash */
{
    power_on();                                          /* melodee_init: the parts' default sounds */
    memset(nor, 255, sizeof nor);
    persist_boot();
    hy_bank_boot();
    settings_init();
    state_boot();
    usb.up = usb.config = 1;                             /* enumerated: WATCH and the native senders need it */
    bridge_tick(1000);
    bridge_drain();
}

int main(void)
{
    static char line[1u << 16];
    bridge_boot();
    fflush(stdout);
    while (fgets(line, sizeof line, stdin)) {
        size_t n = strlen(line);
        const char *err;
        char *s = line;
        if (n && line[n - 1u] != '\n' && !feof(stdin)) {
            int c;
            while ((c = getchar()) != EOF && c != '\n')
                ;
            err = "line too long";
        } else {
            while (n && isspace((unsigned char)line[n - 1u]))
                line[--n] = 0;
            while (*s == ' ' || *s == '\t')
                s++;
            if (!*s)
                continue;
            err = *s == '#' ? line_control(s) : line_sysex(s);
        }
        bridge_drain();
        if (err)
            printf("#error %s\n", err);
        else
            puts("#ok");
        fflush(stdout);
    }
    return 0;
}
