/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* A voice given up for a new note leaves without being heard to go: it keeps rendering down the S curve under the new note,
 * the CPU's budget holds while it does, and a note waits only when there is no slot or budget for it to start beside. */
#include <assert.h>
#include <math.h>
#define MELODEE_RAM_AUDIO 0                          /* the budget as set, not the single-core target's cap of 16 */
#define VBUDGET 18u                                  /* as on the device with CPU1: voices leave room for a fade */
#include "synth_harness.c"

static const struct { const char *name; uint32_t engine, preset; int beside; } ENGINE[] = {
    {"PROPHET", ENGI_PROPHET, 0, 1}, {"JUNO-106", ENGI_J106, 1, 1}, {"CZ-1", ENGI_CZ, 0, 1},
    {"FM6", ENGI_FM6, 0, 0},                         /* all 16 voices of state in use: the note waits */
};
static int bad;
static void ok(const char *engine, const char *what, int pass)
{
    if (!pass) { printf("voice steal: %s: %s: FAIL\n", engine, what); bad++; }
}
static void tick(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
static void ticks(uint32_t n) { while (n--) tick(); }
static int plays(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].stage != 4u && t->v[i].note == note) return 1;
    return 0;
}
static uint32_t live(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}
static uint32_t fades(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage == 4u;
    return n;
}
static voice_t *fading(track_t *t)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].stage == 4u) return &t->v[i];
    return 0;
}
static void start(uint32_t engine, uint32_t preset, uint32_t mode)
{
    power_on();
    vage = 0;                                        /* (a voice's drift comes from its age: runs compare) */
    settings_budget = VOICE_BUDGET_DEFAULT;          /* (power_on keeps the device settings) */
    host_preset(&trk[0], engine, preset);
    host_preset(&trk[1], engine, preset);
    song.g[G_PMODE] = (int16_t)mode;
    ticks(8);
    panic_req = 0;
}
/* every key held until the part has no more room: the next note has to take a voice */
static uint32_t fill(track_t *t, uint32_t first)
{
    uint32_t note = first;
    for (uint32_t i = 0; i < NVOICE; i++, note += 2u) {
        trk_note_on(t, note, 100);
        if (fading(t) || t->wait_n) break;
        ticks(16);
    }
    ticks(VOICE_FADE_BLOCKS + 2u);
    return note + 2u;
}
/* the fade from here to its end: only ever down, in small steps, to nothing, and the CPU's budget holds */
static uint32_t watch_fade(const char *name, voice_t *v, int32_t most)
{
    int32_t was = v->env_out, biggest = 0;
    uint32_t blocks = 0;
    while (v->stage == 4u && v->fade && blocks < 128u) {
        tick();
        int32_t step = was - v->env_out;
        ok(name, "the fade only ever falls", step >= 0);
        if (step > biggest) biggest = step;
        was = v->env_out;
        blocks++;
        ok(name, "the CPU's budget holds through the fade", voices_busy() <= voice_ceiling());
    }
    ok(name, "the fade ends at nothing", was == 0);
    ok(name, "no step of the fade is a cut", biggest <= most);
    return blocks;
}

/* The heart of it: where there is room the note starts at once, and the voice takes its long fade under it */
static void one_part(const char *name, uint32_t engine, uint32_t preset, int beside)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    uint32_t note = fill(t, 48);
    ok(name, "the part is full", !t->wait_n && !fading(t));
    uint32_t kills = voice_kills, held = live(t);
    trk_note_on(t, note, 100);
    voice_t *v = fading(t);
    ok(name, "the new note takes a voice, which starts to fade", v && voice_kills == kills + 1u && v->fade == VOICE_FADE_BLOCKS);
    if (!v) return;
    if (beside) {
        ok(name, "the new note starts at once, beside the fade", plays(t, note) && !t->wait_n);
        ok(name, "as many notes as before", live(t) == held);
        ok(name, "the fade is long", watch_fade(name, v, 32767 / 16) == VOICE_FADE_BLOCKS);
    } else {
        ok(name, "with no slot the new note waits", !plays(t, note) && t->wait_n == 1u);
        ok(name, "and the fade hurries to let it in", watch_fade(name, v, 32767 / 4) <= VOICE_FADE_BLOCKS / VOICE_FADE_HURRY);
    }
    tick();
    ok(name, "the note plays", plays(t, note) && !t->wait_n);
    ok(name, "the voice is gone", !fades(t));
    ok(name, "within the budget", voices_busy() <= voice_budget());
}

/* What was heard: tails ringing, one very light touch. The note sounds at once, one tail fades long, the others go on */
static void light_touch(const char *name, uint32_t preset)
{
    track_t *t = &trk[0];
    start(ENGI_PROPHET, preset, PT_ONE);
    uint32_t note = fill(t, 48);
    for (uint32_t n = 48; n < note; n += 2u) trk_note_off(t, n);
    ticks(4);
    uint32_t tails = 0, kills = voice_kills;
    for (uint32_t i = 0; i < NVOICE; i++) tails += t->v[i].active && !t->v[i].gate;
    ok(name, "the tails ring", tails >= 2u);
    trk_note_on(t, note, 1);
    voice_t *v = fading(t);
    ok(name, "one tail goes", v && fades(t) == 1u && voice_kills == kills + 1u);
    ok(name, "the touch sounds at once", plays(t, note) && !t->wait_n);
    trk_note_off(t, note);                           /* gone again at once: it has sounded, it still releases */
    ok(name, "a touch let go at once still released", !plays(t, note));
    if (v) ok(name, "the tail's fade stays long", watch_fade(name, v, 32767 / 16) == VOICE_FADE_BLOCKS);
    ok(name, "no other tail was touched", voice_kills == kills + 1u);
}

/* A key let go while its note waits must not start or hang; the voice goes on fading */
static void early_release(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    uint32_t note = fill(t, 48);
    trk_note_on(t, note, 100);
    trk_note_on(t, note + 2u, 100);
    ok(name, "the second note at once waits", t->wait_n >= 1u && !plays(t, note + 2u));
    trk_note_off(t, note);
    trk_note_off(t, note + 2u);
    ok(name, "a note let go while it waits is forgotten", !t->wait_n);
    ticks(VOICE_FADE_BLOCKS + 2u);
    ok(name, "and never sounds", !plays(t, note + 2u));
    /* panic while a note waits: nothing starts after it */
    start(engine, preset, PT_ONE);
    note = fill(t, 48);
    trk_note_on(t, note, 100);
    trk_note_on(t, note + 2u, 100);
    panic_req = 1u;
    ticks(VOICE_FADE_BLOCKS + 2u);
    ok(name, "a panic forgets the waiting note", !t->wait_n && !plays(t, note + 2u));
}

/* A chord at once: each note takes one voice, the fades hurry, and all of it plays within 5.8 ms */
static void chord(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    uint32_t note = fill(t, 36);
    uint32_t want = live(t) > 3u ? 3u : live(t) - 1u;
    for (uint32_t i = 0; i < want; i++) trk_note_on(t, note + i * 3u, 100);
    ok(name, "a chord takes one voice a note", fades(t) == want);
    for (uint32_t b = 0; b < VOICE_FADE_BLOCKS / VOICE_FADE_HURRY; b++) {
        tick();
        ok(name, "and stays within the CPU's budget", voices_busy() <= voice_ceiling());
    }
    tick();
    int all = !t->wait_n;
    for (uint32_t i = 0; i < want; i++) all &= plays(t, note + i * 3u);
    ok(name, "and its notes play within 5.8 ms", all);
    ok(name, "the bass stays", plays(t, 36));
}

/* More notes than voices: no note waits only to be stolen by the next. The bass and the last notes play, and no fade follows */
static void over_capacity(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    uint32_t note = fill(t, 36), held = live(t), want = held + 3u;
    if (want + 1u > NVOICE) return;                  /* (FM6 has 16 slots: more keys than the test's range) */
    for (uint32_t i = 0; i < want; i++) trk_note_on(t, note + i, 100);
    uint32_t kills = voice_kills;
    ticks(VOICE_FADE_BLOCKS / VOICE_FADE_HURRY + 2u);
    ok(name, "more notes than voices: nothing left waiting", !t->wait_n);
    ok(name, "the bass stays", plays(t, 36));
    int last = 1;
    for (uint32_t i = 0; i < held - 1u; i++) last &= plays(t, note + want - 1u - i);
    ok(name, "the last notes play", last);
    ticks(VOICE_FADE_BLOCKS);
    ok(name, "and no fade follows", voice_kills == kills);
}

/* Two parts: a voice from the other part fades the same way, under the note */
static void two_parts(const char *name, uint32_t engine, uint32_t preset)
{
    start(engine, preset, PT_MULTI);
    uint32_t units = voice_units(&trk[0]);
    settings_budget = (int16_t)(4u * units);
    for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[1], 48u + i * 2u, 100);
    ticks(16);
    uint32_t kills = voice_kills;
    trk_note_on(&trk[0], 72, 100);
    voice_t *v = fading(&trk[1]);
    ok(name, "a part within its share takes from one over it", v && voice_kills == kills + 1u);
    ok(name, "its note starts at once", plays(&trk[0], 72));
    if (v) ok(name, "the voice's fade is long", watch_fade(name, v, 32767 / 16) == VOICE_FADE_BLOCKS);
    /* a note claims no more than its share: the waiting and the sounding count */
    start(engine, preset, PT_MULTI);
    settings_budget = (int16_t)(4u * units);
    for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[1], 48u + i * 2u, 100);
    ticks(16);
    for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[0], 72u + i * 2u, 100);
    ok(name, "the other part keeps its guaranteed share", live(&trk[1]) >= 2u);
    ticks(VOICE_FADE_BLOCKS + 4u);
    ok(name, "and the budget holds at the end", voices_busy() <= voice_budget());
}

/* Two parts on the device's budget: they leave one voice's room, so even at full load a fade can run under its note */
static void two_parts_full(void)
{
    start(ENGI_J106, 1, PT_MULTI);                   /* two units a voice, as a Prophet voice with CPU1 */
    uint32_t units = voice_units(&trk[0]);
    for (uint32_t i = 0; i < 4u; i++) { trk_note_on(&trk[0], 48u + i, 100); trk_note_on(&trk[1], 60u + i, 100); }
    ticks(16);
    ok("two parts", "both parts hold their share", live(&trk[0]) == 4u && live(&trk[1]) == 4u);
    ok("two parts", "and one voice's room is left", voices_busy() + units <= voice_ceiling());
    trk_note_on(&trk[0], 70, 100);
    ok("two parts", "a further note starts at once", plays(&trk[0], 70) && !trk[0].wait_n && fades(&trk[0]) == 1u);
    ticks(VOICE_FADE_BLOCKS + 2u);
}

/* MONO takes its one voice from the other part the same way */
static void mono(const char *name, uint32_t engine, uint32_t preset)
{
    start(engine, preset, PT_MULTI);
    uint32_t units = voice_units(&trk[0]);
    settings_budget = (int16_t)(4u * units);
    trk[0].p[P_VOICE] = V_MONO;
    trk[0].p[P_PRIO] = 0;
    for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[1], 48u + i * 2u, 100);
    ticks(16);
    trk_note_on(&trk[0], 72, 100);
    ok(name, "MONO: a voice goes for the note", fading(&trk[1]) != 0);
    ok(name, "MONO: and it plays at once", plays(&trk[0], 72));
    ticks(VOICE_FADE_BLOCKS + 1u);
    ok(name, "MONO: within the budget", voices_busy() <= voice_budget());
    trk_note_off(&trk[0], 72);
}

/* A held voice fading: its key is not held any more for anything that asks, and letting it go changes nothing */
static void held_fade(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    trk_note_on(t, 60, 100);
    trk_note_on(t, 64, 100);
    ticks(16);
    voice_t *v = 0;
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].note == 64u && t->v[i].active) v = &t->v[i];
    if (!v) { ok(name, "the note sounds", 0); return; }
    voice_kill(v);
    ok(name, "a fading voice stays gated for its engine", v->gate);
    ok(name, "but is not the bass", lowest_held(t) != (uint32_t)(v - t->v));
    trk_note_off(t, 64);
    ok(name, "its key-up leaves the fade alone", v->stage == 4u);
    ticks(VOICE_FADE_BLOCKS + 1u);
    ok(name, "and it ends", !v->active && !v->gate);
    /* the same key while its voice fades: a new voice, never a jump back up */
    trk_note_on(t, 67, 100);
    ticks(16);
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].note == 67u && t->v[i].active) v = &t->v[i];
    voice_kill(v);
    ticks(2);
    uint32_t left = v->fade;
    trk_note_on(t, 67, 100);
    ok(name, "a key on its own fading voice does not restart the fade", v->stage == 4u && v->fade == left);
    ok(name, "and it plays", plays(t, 67));
    ticks(VOICE_FADE_BLOCKS + 2u);
    trk_note_off(t, 60);
    trk_note_off(t, 67);
}

/* a fade is not started again, or it would jump back up */
static void refade(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    trk_note_on(t, 60, 100);
    ticks(16);
    voice_kill(&t->v[0]);
    ticks(3);
    uint32_t left = t->v[0].fade, kills = voice_kills;
    voice_kill(&t->v[0]);
    ok(name, "killing a fading voice changes nothing", t->v[0].fade == left && voice_kills == kills);
    trk_all_off(t);
    ok(name, "all notes off leaves a fade going", t->v[0].stage == 4u && t->v[0].fade == left);
    ticks(VOICE_FADE_BLOCKS + 1u);
    ok(name, "and it ends", !t->v[0].active);
}

/* The part's own output: a voice leaves nothing behind it. Its last sample is near nothing, so the part falling silent
 * is no step (a voice's DC on a capacitor after the voices did this) */
static void leaves_nothing(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[1];
    start(engine, preset, PT_MULTI);
    trk_note_on(t, 60, 100);
    int32_t y[CTL];
    double peak = 1, last = 0;
    for (uint32_t b = 0; b < 400u; b++) {
        track_render(t, y, CTL);
        for (uint32_t i = 0; i < CTL; i++) if (fabs((double)y[i]) > peak) peak = fabs((double)y[i]);
    }
    voice_t *v = 0;
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active) v = &t->v[i];
    if (!v) { ok(name, "the note sounds", 0); return; }
    voice_kill(v);
    for (uint32_t b = 0; b < VOICE_FADE_BLOCKS; b++) {
        track_render(t, y, CTL);
        last = fabs((double)y[CTL - 1]);
    }
    ok(name, "the fade's last sample is near nothing (no step when the part falls silent)", last < peak / 50.0);
    ticks(1);
    ok(name, "and the voice is gone", !v->active);
}

/* A key struck again in its own tail: no jump in the output. The engines that restart their envelopes take another voice */
static void repress(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    int32_t y[CTL];
    double peak = 1, norm = 0, prev = 0;
    trk_note_on(t, 60, 100);
    for (uint32_t b = 0; b < 300u; b++) {
        track_render(t, y, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            if (fabs((double)y[i]) > peak) peak = fabs((double)y[i]);
            if (b > 100u && fabs((double)y[i] - prev) > norm) norm = fabs((double)y[i] - prev);
            prev = y[i];
        }
    }
    trk_note_off(t, 60);
    track_render(t, y, CTL);
    prev = y[CTL - 1];
    trk_note_on(t, 60, 100);
    voice_wait_block(t);
    track_render(t, y, CTL);
    ok(name, "a key struck in its tail does not jump", fabs((double)y[0] - prev) <= norm * 1.1 + peak / 100.0);
    ok(name, "and sounds", plays(t, 60));
}

/* A light press on the tail of a hard one, and the reverse: the velocity's gain must not step the output. This was the click:
 * Unicorn Dreams' A4 went from -148 to -17 in one sample. The tail rings on where the engine has velocity */
static void repress_velocity(const char *name, uint32_t engine, uint32_t preset, uint32_t first, uint32_t second)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    int32_t y[CTL];
    double peak = 1, prev = 0, tail = 0, after = 0;
    trk_note_on(t, 69, first);
    for (uint32_t b = 0; b < 300u; b++) {
        track_render(t, y, CTL);
        for (uint32_t i = 0; i < CTL; i++) if (fabs((double)y[i]) > peak) peak = fabs((double)y[i]);
    }
    trk_note_off(t, 69);
    for (uint32_t b = 0; b < 40u; b++) track_render(t, y, CTL);
    prev = y[CTL - 1];
    for (uint32_t i = 0; i < CTL; i++) if (fabs((double)y[i]) > tail) tail = fabs((double)y[i]);
    trk_note_on(t, 69, second);
    track_render(t, y, CTL);
    ok(name, "a re-press at another velocity does not jump", fabs((double)y[0] - prev) < peak * 0.12);
    for (uint32_t i = 0; i < CTL; i++) if (fabs((double)y[i]) > after) after = fabs((double)y[i]);
    if (second < first && ENGINES[engine]->rings_on)
        ok(name, "a light re-press leaves the tail ringing", after > tail * 0.5);
    ok(name, "and it sounds", plays(t, 69));
}

/* MONO's next note at another velocity: no step on the engines that go on in place */
static void mono_velocity(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    t->p[P_VOICE] = V_MONO;
    t->p[P_PRIO] = 0;
    int32_t y[CTL];
    double peak = 1, prev = 0;
    trk_note_on(t, 69, 100);
    for (uint32_t b = 0; b < 300u; b++) {
        track_render(t, y, CTL);
        for (uint32_t i = 0; i < CTL; i++) if (fabs((double)y[i]) > peak) peak = fabs((double)y[i]);
    }
    prev = y[CTL - 1];
    trk_note_on(t, 71, 12);
    track_render(t, y, CTL);
    ok(name, "MONO's light next note does not step", fabs((double)y[0] - prev) < peak * 0.12);
    trk_note_off(t, 69);
    trk_note_off(t, 71);
}

/* MONO's next note: no jump in the output, and it plays */
static void mono_retrigger(const char *name, uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    start(engine, preset, PT_ONE);
    t->p[P_VOICE] = V_MONO;
    t->p[P_PRIO] = 0;
    int32_t y[CTL];
    double peak = 1, norm = 0, prev = 0;
    trk_note_on(t, 60, 100);
    for (uint32_t b = 0; b < 300u; b++) {
        track_render(t, y, CTL);
        for (uint32_t i = 0; i < CTL; i++) {
            if (fabs((double)y[i]) > peak) peak = fabs((double)y[i]);
            if (b > 100u && fabs((double)y[i] - prev) > norm) norm = fabs((double)y[i] - prev);
            prev = y[i];
        }
    }
    prev = y[CTL - 1];
    trk_note_on(t, 62, 100);
    track_render(t, y, CTL);
    ok(name, "MONO's next note does not jump", fabs((double)y[0] - prev) <= norm * 1.1 + peak / 100.0);
    ticks(VOICE_FADE_BLOCKS / VOICE_FADE_HURRY + 2u);
    ok(name, "and it plays within 5.8 ms", plays(t, 62) && !t->wait_n);
    trk_note_off(t, 60);
    trk_note_off(t, 62);
    ok(name, "and ends", !plays(t, 60) && !plays(t, 62));
}

/* The Prophet's coupling sits on the voice: a fast fade moves no DC, so nothing is left once the voice has gone */
static void coupling(const char *name, uint32_t preset)
{
    enum { BLOCKS = VOICE_FADE_BLOCKS + 8u };
    static int32_t with[CTL * BLOCKS], without[CTL * BLOCKS], bass[CTL * BLOCKS];
    int32_t y[CTL], z[CTL];
    double peak = 1, residue = 0;
    for (int run = 0; run < 3; run++) {
        start(ENGI_PROPHET, preset, PT_MULTI);
        settings_budget = (int16_t)(2u * voice_units(&trk[1]));
        trk_note_on(&trk[1], 36, 100);
        if (run < 2) trk_note_on(&trk[1], 64, 120);
        for (uint32_t b = 0; b < 600u; b++) {
            track_render(&trk[1], y, CTL);
            track_render(&trk[0], z, CTL);
        }
        if (run == 0) trk_note_on(&trk[0], 84, 120);
        for (uint32_t b = 0; b < BLOCKS; b++) {
            track_render(&trk[1], y, CTL);
            track_render(&trk[0], z, CTL);
            memcpy((run == 0 ? with : run == 1 ? without : bass) + b * CTL, y, sizeof y);
        }
    }
    for (uint32_t i = 0; i < CTL * BLOCKS; i++) {
        double v = fabs((double)without[i] - bass[i]);
        if (v > peak) peak = v;
    }
    for (uint32_t i = (VOICE_FADE_BLOCKS + 2u) * CTL; i < CTL * BLOCKS; i++) {
        double v = fabs((double)with[i] - bass[i]);
        if (v > residue) residue = v;
    }
    printf("voice steal: %s: left behind after the fade %.2f %% of the voice's peak\n", name, 100.0 * residue / peak);
    ok(name, "a stolen voice leaves nothing on the output (under 0.5 %)", residue < peak / 200.0);
}

int main(void)
{
    int32_t last = 32767;
    for (uint32_t k = 1u; k < VOICE_FADE_BLOCKS; k++) {
        int32_t g = voice_fade_gain(VOICE_FADE_BLOCKS - k);
        ok("curve", "falls", g < last && g > 0);
        last = g;
    }
    ok("curve", "ends at 0", voice_fade_gain(0) == 0);
    ok("curve", "starts level (no corner)", voice_fade_gain(VOICE_FADE_BLOCKS - 1u) > 32767 * 99 / 100);
    ok("curve", "ends level (no corner)", voice_fade_gain(1u) < 32767 / 100);
    for (uint32_t k = 0; k < NELEM(ENGINE); k++) {
        const char *n = ENGINE[k].name;
        uint32_t e = ENGINE[k].engine, p = ENGINE[k].preset;
        one_part(n, e, p, ENGINE[k].beside);
        early_release(n, e, p);
        chord(n, e, p);
        over_capacity(n, e, p);
        two_parts(n, e, p);
        mono(n, e, p);
        held_fade(n, e, p);
        refade(n, e, p);
        leaves_nothing(n, e, p);
        repress(n, e, p);
        mono_retrigger(n, e, p);
        repress_velocity(n, e, p, 100, 12);
        repress_velocity(n, e, p, 12, 100);
        mono_velocity(n, e, p);
    }
    two_parts_full();
    /* the patches this was heard on: long tails on a dark tone */
    repress_velocity("Unicorn Dreams", ENGI_PROPHET, 27, 100, 12);
    repress_velocity("Unicorn Dreams", ENGI_PROPHET, 27, 12, 100);
    repress_velocity("Forever Keys", ENGI_PROPHET, 3, 100, 12);
    repress_velocity("Forever Keys", ENGI_PROPHET, 3, 12, 100);
    mono_velocity("Unicorn Dreams", ENGI_PROPHET, 27);
    light_touch("Forever Keys", 3);
    light_touch("Unicorn Dreams", 27);
    one_part("Forever Keys", ENGI_PROPHET, 3, 1);
    one_part("Unicorn Dreams", ENGI_PROPHET, 27, 1);
    leaves_nothing("Unicorn Dreams", ENGI_PROPHET, 27);
    coupling("Unicorn Dreams", 27);
    coupling("Forever Keys", 3);
    coupling("INIT PROPHET", 0);
    if (bad) { printf("VOICE STEAL TEST FAILED (%d)\n", bad); return 1; }
    puts("voice steal: long fades under the note, hurried fades, budget, two parts, MONO: ok");
    return 0;
}
