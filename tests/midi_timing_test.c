/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Melodee 0e3d353 regression coverage adapted to Saadet's single synth and
 * production USB/TRS/BLE ingress. Wall milliseconds and TIMER4 wrap separately. */
#include <assert.h>
static unsigned timing_now;
#define MIDI_TIME_NOW() timing_now
#define MIDI_TICKS_PER_MS 24000u
#define MELODEE_USB_AUDIO 0
#include "synth_harness.c"

static int sd_ble_connected(void) { return 1; }
static int sd_ble_device_state(void) { return 4; }
static uint8_t settings_thru;
static void sd_midi_thru(uint32_t packet, int port) { (void)packet; (void)port; }
#include "../platform/midi_ingress.h"

static uint32_t legacy;
static void packet(uint32_t start, uint64_t elapsed, uint32_t status, uint32_t source)
{
    timing_now = host_ticks = start + (uint32_t)elapsed;
    fm1_ms = 1000u + (uint32_t)(elapsed / MIDI_TICKS_PER_MS);
    uint32_t p = (status >= 0xF8u ? 15u : status >> 4) | status << 8 | 60u << 16 | 100u << 24;
    if (legacy) midi_enqueue(p, source);
    else sd_ingress(p, source);
}
static void reset(uint32_t source, uint32_t start)
{
    power_on();
    midi_sync_active = 0;
    midi_beat_samples = beat_pos = beat_n = 0;
    midi_render_timed = 0;
    memset(midi_rx_pulses, 0, sizeof midi_rx_pulses);
    memset(sd_midi_keys, 0, sizeof sd_midi_keys);
    memset(sd_midi_pedal, 0, sizeof sd_midi_pedal);
    song.g[G_CLOCK] = source == 4u ? 3 : source;
    events_block(0);
    packet(start, 0, 0xFA, source); events_block(0);
}
static void phase(uint32_t pulse)
{
    assert(song.playing);
    assert(midi_clock.pos == pulse * 1024u);
    assert(beat_pos == pulse % 24u * 1024u);
    assert(beat_n == pulse / 24u % 4u);
}
static void overflow(uint32_t source)
{
    const uint32_t start = 0xFFF00000u, interval = 500000u;
    reset(source, start);
    uint32_t clock_source = source == 4u ? 3u : source;
    midi_rx_pulses[clock_source - 1u] = 65534u;
    packet(start, 0, 0xFA, source); events_block(0);
    for (uint32_t pulse = 0; pulse <= 96u; pulse++) {
        uint64_t at = (uint64_t)pulse * interval;
        if (pulse % 24u == 0u) {
            packet(start, at, 0x90, source); events_block(0);
            for (uint32_t i = 0; i < MQ - 1u; i++) packet(start, at, 0xB0, source);
            packet(start, at, 0xF8, source); /* admitted, then discarded */
            packet(start, at, 0xB0, source); assert(midi_in_overflow);
        } else packet(start, at, 0xF8, source); /* also count rejected clocks */
        packet(start, at, 0xF8, clock_source == 1u ? 2u : 1u);
        if (pulse % 24u < 3u) continue;
        events_block(0);
        assert(!midi_held_count(0) && !TSEL->nheld && !TSEL->arp_note);
        if (pulse % 24u != 3u) phase(pulse);
    }
    events_block(0);
    for (uint32_t pulse = 97u; pulse <= 120u; pulse++) {
        packet(start, (uint64_t)pulse * interval, 0xF8, source); events_block(0); phase(pulse);
    }
    assert(song.g[G_BPM] == 120 && midi_beat_samples == FS / 2u);

    /* Continue snapshots a new baseline; pulses received while stopped cannot
     * become a giant catch-up. A cable timeout also discards old count gaps. */
    packet(start, 121u * interval, 0xFC, source); events_block(0);
    for (uint32_t i = 122u; i < 150u; i++) {
        packet(start, (uint64_t)i * interval, 0xF8, source); events_block(0);
    }
    uint32_t pos = midi_clock.pos;
    packet(start, 150u * interval, 0xFB, source); events_block(0);
    packet(start, 150u * interval, 0xF8, source); events_block(0);
    assert(midi_clock.pos == midi_clock.rendered);
    assert(midi_clock.pos >= pos && midi_clock.pos - pos < 1024u);
    pos = midi_clock.pos;
    packet(start, 151u * interval, 0xF8, source); events_block(0);
    assert(midi_clock.pos == pos + 1024u);
    midi_rx_pulses[clock_source - 1u] += 300u;
    packet(start, 200u * interval, 0xF8, source); events_block(0);
    assert(midi_clock.pos == pos + 2048u);
}
static void long_session(uint32_t source)
{
    const uint32_t start = 0xFFF00000u, bpm = 137u, clocks = bpm * 24u * 120u;
    reset(source, start);
    for (uint32_t pulse = 0; pulse <= clocks; pulse++) {
        uint64_t elapsed = (uint64_t)pulse * 60000u * MIDI_TICKS_PER_MS / (bpm * 24u);
        packet(start, elapsed, 0xF8, source); events_block(0); phase(pulse);
        timing_now += 30000u * MIDI_TICKS_PER_MS / (bpm * 24u);
        events_block(0);
    }
}
static void arp_catchup(void)
{
    for (uint32_t swing = 0; swing <= 100u; swing += 100u) {
        reset(2, 24000000u);
        track_t *t = TSEL;
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 3; t->p[P_ASWING] = swing;
        t->p[P_AOCT] = 1; t->p[P_APROB] = 127; t->p[P_AGATE] = 64;
        t->nheld = 3; t->held[0] = 60; t->held[1] = 64; t->held[2] = 67;
        t->arp_idx = UINT32_MAX; t->arp_pos = 0xFFFFFFFu;
        arp_tick(t, 0);
        uint32_t elapsed = 4u * 3072u + 321u;
        for (uint32_t n = elapsed; n;) {
            uint32_t chunk = n < 32u ? n : 32u;
            arp_tick(t, chunk); n -= chunk;
        }
        uint32_t idx = t->arp_idx, pos = t->arp_pos, off = t->arp_off, note = t->arp_note;
        if (t->arp_note) arp_off(t);
        t->arp_idx = UINT32_MAX; t->arp_pos = 0xFFFFFFFu;
        arp_tick(t, 0); arp_tick(t, elapsed);
        assert(t->arp_idx == idx && t->arp_pos == pos && t->arp_off == off && t->arp_note == note);
    }
}
int main(void)
{
    for (legacy = 0; legacy <= 1u; legacy++) {
        for (uint32_t source = 1; source <= (legacy ? 2u : 4u); source++) {
            overflow(source); long_session(source);
        }
    }
    arp_catchup();
    puts("MIDI timing: USB/TRS/BLE overflow, two-hour wraps, transport and swung ARP catch-up passed");
    return 0;
}
