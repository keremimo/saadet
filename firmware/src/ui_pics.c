/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The pictures between a page's header and its four values: what the values do, drawn from the sound itself (the
 * envelope's shape, the LFO's wave, the routes, the arp's steps, the scale, the chord, the voices, a filter's curve,
 * FM6's algorithm, an operator's or a CZ-1 line's envelope) or the four values as faders; on the GLOBAL pages the first
 * value large. pic_build puts a picture's data into the view (pv, ps), so the picture is
 * drawn again whenever what it shows changes (an arp step, a voice starting); pic_draw draws it from the view alone.
 * The area: y 30 .. 172, between the header and the column labels. */

/* -------------------------------------------------------------- data --- */
static void pic_arp(ui_vm_t *v, const track_t *t)   /* the 16 steps the pattern plays over the notes held */
{
    static const uint8_t EXAMPLE[3] = {60, 64, 67};   /* (nothing held: a C triad) */
    uint32_t list[64], cnt = t->nheld ? t->nheld : 3u, len = 0, i, j, o, lo = 127, hi = 0;
    for (i = 0; i < cnt; i++)
        list[i] = t->nheld ? t->held[i] : EXAMPLE[i];
    if (!t->p[P_AORDER])                                   /* (as seq.c arp_step: by pitch, or as played) */
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1u] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1u];
                list[j - 1u] = x;
            }
    for (o = 0; o < (uint32_t)clamp(t->p[P_AOCT], 1, 4); o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (t->p[P_AMODE] == 6) {                               /* REPEAT: the last note played */
        list[0] = t->nheld ? t->held[t->nheld - 1u] : EXAMPLE[2];
        len = 1;
    }
    for (i = 0; i < 16u; i++) {
        switch (t->p[P_AMODE]) {
        case 2: j = len - 1u - i % len; break;
        case 3: { uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = i % cyc; j = k < len ? k : cyc - k; break; }
        case 4: j = (i * 5u + 3u) % len; break;         /* RND: one random-looking pattern */
        default: j = i % len; break;
        }
        v->pv[i] = (int16_t)list[j];
        if (list[j] < lo) lo = list[j];
        if (list[j] > hi) hi = list[j];
    }
    for (i = 0; i < 16u; i++)
        v->pv[i] = (int16_t)(v->pv[i] - (int32_t)lo);
    v->pv[16] = (int16_t)(hi - lo);
    v->pv[17] = (int16_t)(t->p[P_AMODE] && t->nheld ? (int32_t)(t->arp_idx % 16u) : -1);   /* the playhead */
    v->pv[18] = t->p[P_AGATE];
    v->pv[19] = t->p[P_ASWING];
    v->pv[20] = t->p[P_AMODE];
    fmt_int(v->ps[0], song.g[G_BPM]);
    str_cpy(v->ps[0] + str_len(v->ps[0]), " BPM", 8);
}
static void pic_route(char *b, const track_t *t, uint32_t k)   /* "LFO > PITCH", the engine's name for E1..E8 */
{
    int32_t src = t->p[P_M1SRC + 3u * k], dst = t->p[P_M1DST + 3u * k];
    const char *d = dst >= 12 && dst <= 19 ? track_desc(t, P_E0 + (uint32_t)(dst - 12))->label : 0;
    if (!src || !dst) {
        str_cpy(b, "OFF", 24);
        return;
    }
    str_cpy(b, N_MSRC[clamp(src, 0, (int32_t)NELEM(N_MSRC) - 1)], 24);
    str_cpy(b + str_len(b), " \x9b ", 24 - str_len(b));
    str_cpy(b + str_len(b), d && d[0] && !str_eq(d, "-") ? d : N_MDST[clamp(dst, 0, (int32_t)NELEM(N_MDST) - 1)],
            24 - str_len(b));
}
static void hylab_pic(ui_vm_t *v, const ui_stop_t *st);   /* hydra_lab.c: HYDRA's slots, the WAVE LAB */
static void hylab_draw(const ui_vm_t *v);
static void pic_build(ui_vm_t *v, const ui_stop_t *st)
{
    const track_t *t = TSEL;
    uint32_t k;
    v->pic = st->pic;
    switch (st->pic) {
#ifdef SAADET_FM1_BASE
    case PIC_BLE: {
        int state = sd_ble_state();
        const char *status = !sd_ble_available() ? "UNAVAILABLE" : !sd_ble_enabled() ? "OFF" :
            state == 0 ? "STOPPED" : state == 1 ? "STARTING" : state == 3 ? "FAILED" :
            sd_ble_connected() ? "MIDI CONNECTED" : sd_ble_linked() ? "WAITING FOR MIDI" :
            sd_ble_device_state() >= 2 ? "DEVICE LINK IN USE" : "ADVERTISING";
        str_cpy(v->ps[0], status, sizeof v->ps[0]);
        str_cpy(v->ps[1], "Saadet", sizeof v->ps[1]);
        if (sd_ble_linked() && !sd_ble_connected()) {
            str_cpy(v->ps[2], "Mac: reopen", sizeof v->ps[2]);
            str_cpy(v->ps[3], "Audio MIDI Setup", sizeof v->ps[3]);
        } else if (sd_ble_connected()) {
            str_cpy(v->ps[2], "MIDI in / out ready", sizeof v->ps[2]);
        } else if (sd_ble_device_state() >= 2) {          /* one Bluetooth link at a time: the device's */
            str_cpy(v->ps[2], "Disconnect the device", sizeof v->ps[2]);
            str_cpy(v->ps[3], "on DEV to connect here", sizeof v->ps[3]);
        } else {
            str_cpy(v->ps[2], "Connect from your", sizeof v->ps[2]);
            str_cpy(v->ps[3], "Bluetooth MIDI app", sizeof v->ps[3]);
        }
        if(sd_ble_forget_state()){
            str_cpy(v->ps[0],sd_ble_forget_state()==1?"FORGET ALL PAIRINGS?":"FORGETTING...",sizeof v->ps[0]);
            str_cpy(v->ps[2],sd_ble_forget_state()==1?"SAVE confirms":"Closing the link",sizeof v->ps[2]);
            str_cpy(v->ps[3],sd_ble_forget_state()==1?"K3 back cancels":"",sizeof v->ps[3]);
        }
        break;
    }
    case PIC_BTDEV: {
        int state=sd_ble_device_state(),n=sd_ble_device_count(),pick=sd_ble_device_pick();
        const char *status=sd_ble_state()!=2?"RADIO NOT READY":state==4?"CONNECTED":
            state==2?"CONNECTING":state==3?"FINDING MIDI":sd_ble_linked()?"COMPUTER CONNECTED":
            n?"MIDI DEVICES":"LOOKING FOR DEVICES";
        str_cpy(v->ps[0],status,sizeof v->ps[0]);
        v->pv[0]=(int16_t)pick;v->pv[1]=(int16_t)n;v->pv[2]=(int16_t)state;
        if(state>=2&&state<=4){
            str_cpy(v->ps[1],sd_ble_device_name(),sizeof v->ps[1]);
            str_cpy(v->line[0],"SAVE: DISCONNECT",sizeof v->line[0]);
            int32_t code=state==3?sd_ble_pairing_code():-1;
            if(code>=0&&code<=999999){
                str_cpy(v->ps[0],"PAIRING CODE",sizeof v->ps[0]);
                for(int i=5;i>=0;i--){v->ps[1][i]=(char)('0'+code%10);code/=10;}
                v->ps[1][6]=0;
                str_cpy(v->ps[2],"Enter on other device",sizeof v->ps[2]);
            }
        }else{
            int first=pick>1?pick-1:0;
            if(first+3>n)first=n>3?n-3:0;
            v->pv[3]=(int16_t)first;
            for(int i=0;i<3;i++){
                int signal=0;
                if(sd_ble_device_found(first+i,v->ps[i+1],&signal))v->pv[4+i]=(int16_t)signal;
            }
            str_cpy(v->line[0],n?"SAVE: CONNECT":"Put device in pairing mode",sizeof v->line[0]);
        }
        break;
    }
#endif
    case PIC_ENV:
        v->pv[0] = t->p[P_ATK]; v->pv[1] = t->p[P_DEC]; v->pv[2] = t->p[P_SUS]; v->pv[3] = t->p[P_REL];
        break;
    case PIC_HYSLOTS: case PIC_HYWAVE: case PIC_HYSPEC: case PIC_HYNAME:
        hylab_pic(v, st);
        break;
    case PIC_LFO:
        v->pv[0] = t->p[P_LWAVE]; v->pv[1] = t->p[P_LFADE]; v->pv[2] = t->p[P_LPOL]; v->pv[3] = t->p[P_LPHASE];
        break;
    case PIC_MATRIX:
        for (k = 0; k < 4u; k++) {
            pic_route(v->ps[k], t, k);
            v->pv[k] = t->p[P_M1SRC + 3u * k] && t->p[P_M1DST + 3u * k] ? t->p[P_M1AMT + 3u * k] : 0;
        }
        v->pv[4] = (int16_t)((int32_t)v->dot - 1);     /* the route of its page (AMOUNTS: none) */
        break;
    case PIC_ARP:
        pic_arp(v, t);
        break;
    case PIC_SCALE:
        v->pv[0] = t->p[P_SCALE]; v->pv[1] = t->p[P_ROOT]; v->pv[2] = t->p[P_QUANT]; v->pv[3] = t->p[P_TRANS];
        break;
    case PIC_CHORD: {                                     /* the last chord played, or the one the root would play */
        uint8_t out[CHORD_MAX];
        int32_t r;
        uint16_t mask;
        uint32_t n, i;
        if (!t->p[P_CHRD]) {
            str_cpy(v->ps[0], "OFF", 24);
            break;
        }
        if (chord_last[0].n > 1u) {
            n = chord_last[0].n;
            for (i = 0; i < n; i++) out[i] = chord_last[0].note[i];
            r = chord_last[0].root;
            mask = chord_last[0].mask;
        } else {
            n = chord_make(t, 60u + (uint32_t)t->p[P_ROOT] % 12u, out, &r, &mask);
        }
        chord_name(v->ps[0], (uint32_t)r, mask | 1u);
        for (i = 0; i < n && i < 4u; i++) v->pv[i] = out[i];
        v->pv[4] = (int16_t)n;
        v->pv[5] = (int16_t)(out[0] / 12u * 12u);          /* the strip starts at the C below the lowest note */
        break;
    }
    case PIC_VOICES: {
        const engine_t *e = ENGINES[eng_idx(t->eng_req)];
        uint32_t cap = e->cap ? e->cap(t) : e->poly ? e->poly : NPOLY;
        v->pv[0] = (int16_t)(cap > NVOICE ? NVOICE : cap);
        v->pv[1] = t->p[P_VOICE];
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active) v->pv[2] = (int16_t)(v->pv[2] | (int16_t)(1u << k));
        break;
    }
    case PIC_P5ENV: {                                     /* the Prophet's filter (arg 0) or amp envelope, 0..127 */
        if (st->arg == 2u) {                              /* (arg 2: the JUNO-106's one envelope) */
            const uint8_t *j = j106_patch_of(t)->raw;
            v->pv[0] = j[J106_ENV_A]; v->pv[1] = j[J106_ENV_D]; v->pv[2] = j[J106_ENV_S]; v->pv[3] = j[J106_ENV_R];
            break;
        }
        if (st->arg == 3u) {                              /* (arg 3: HYDRA's ENV 1) */
            const uint8_t *h = hy_patch_of(t)->raw;
            v->pv[0] = h[HY_E1_ATK]; v->pv[1] = h[HY_E1_DEC]; v->pv[2] = h[HY_E1_SUS]; v->pv[3] = h[HY_E1_REL];
            break;
        }
        const uint8_t *p = p5_patch_of(t)->raw;
        uint32_t a = st->arg ? P5_ATTACK_AMP : P5_ATTACK_FILTER;
        v->pv[0] = p[a]; v->pv[1] = p[a + 2u]; v->pv[2] = p[a + 4u];
        v->pv[3] = p[P5_RELEASE_ON] ? p[a + 6u] : 16;   /* (RELEASE off: the short release, as eng_prophet) */
        break;
    }
    case PIC_FILTER:                                      /* the cut 0..127 and the resonance 0..127 (SID: arg 1) */
        if (st->arg == 2u) {                              /* (JUNO-106) */
            v->pv[0] = j106_patch_of(t)->raw[J106_VCF_FREQ]; v->pv[1] = j106_patch_of(t)->raw[J106_VCF_RES];
        } else if (st->arg == 3u) {                       /* (HYDRA's FILTER 1) */
            v->pv[0] = hy_patch_of(t)->raw[HY_F1_CUT]; v->pv[1] = hy_patch_of(t)->raw[HY_F1_RES];
        } else if (st->arg) {
            v->pv[0] = t->p[P_E4]; v->pv[1] = (int16_t)(t->p[P_E5] * 127 / 15);
        } else {
            v->pv[0] = p5_patch_of(t)->raw[P5_CUTOFF]; v->pv[1] = p5_patch_of(t)->raw[P5_RESONANCE];
        }
        break;
    case PIC_ALGO: {                                      /* the algorithm, the operator of the page (6: none) */
        const uint8_t *p = fm6_patch[song.sel % NTRK];
        v->pv[0] = (int16_t)(p[FP_ALG] & 31u); v->pv[1] = st->arg; v->pv[2] = fm6_on[song.sel % NTRK];
        v->pv[3] = (int16_t)(p[FP_FB] & 7u);
        break;
    }
    case PIC_FMEG: {                                      /* R1..R4, L1..L4 of an operator, or the pitch EG (arg 6) */
        const uint8_t *p = st->arg < 6u ? &fm6_patch[song.sel % NTRK][(5u - st->arg) * FP_OP + FP_R1]
                                        : &fm6_patch[song.sel % NTRK][FP_PR1];
        for (k = 0; k < 8u; k++) v->pv[k] = p[k];
        v->pv[8] = st->arg == 6u;
        break;
    }
    case PIC_CZEG: {                                      /* R1..R8, L1..L8, SUS, END of line arg / 3, envelope arg % 3 */
        uint8_t p[LCZ_PACKED];
        cz_ed_decode(p, cz_patch[song.sel % NTRK].raw);
        for (k = 0; k < 18u; k++) v->pv[k] = p[LCZ_EBASE(st->arg / 3u, st->arg % 3u) + k];
        break;
    }
    }
}

/* ------------------------------------------------------------ drawing --- */
static void pic_env(const ui_vm_t *v)
{
    int32_t a = 6 + v->pv[0] * 50 / 127, d = 6 + v->pv[1] * 50 / 127, r = 6 + v->pv[3] * 50 / 127;
    int32_t x1 = 12 + a, x2 = x1 + d, x3 = 228 - r, ys = 160 - v->pv[2] * 112 / 127;
    if (x3 < x2 + 16) x3 = x2 + 16;
    int32_t xy[10] = {12 * 16, 160 * 16, x1 * 16, 48 * 16, x2 * 16, ys * 16, x3 * 16, ys * 16, 228 * 16, 160 * 16};
    cv_rect(12, 161, 216, 1, T_LINE);
    cv_poly(xy, 5, 32, T_THEME, 3u);
    for (uint32_t i = 1; i < 4u; i++)
        cv_disc(xy[2 * i] / 16, xy[2 * i + 1] / 16, 7, T_THEME, 0);
}
static void pic_lfo(const ui_vm_t *v)
{
    int32_t xy[2 * 109], i, wave = v->pv[0], uni = v->pv[2];
    float fade = (float)v->pv[1] * 216.0f / 127.0f;
    cv_rect(12, uni ? 156 : 100, 216, 1, T_LINE);
    for (i = 0; i < 109; i++) {
        float ph = (float)i / 54.0f + (float)v->pv[3] / 128.0f, f = ph - fm_floorf(ph), w, amp;
        switch (wave) {
        case 0: w = fm_sinf(FM_TWO_PI * ph); break;                       /* SIN */
        case 1: w = 1.0f - 4.0f * fm_fabsf(f - 0.5f); w = -w; break;      /* TRI */
        case 2: w = 2.0f * f - 1.0f; break;                               /* SAW */
        case 3: w = f < 0.5f ? 1.0f : -1.0f; break;                       /* SQR */
        default: {                                                         /* S&H: a held random value a step */
            uint32_t h = (uint32_t)(fm_floorf(ph * 4.0f) + 64.0f) * 2654435761u;
            w = (float)((h >> 24) & 255u) / 127.5f - 1.0f;
            break;
        }
        }
        amp = fade > 0.0f && (float)(2 * i) < fade ? (float)(2 * i) / fade : 1.0f;
        xy[2 * i] = (12 + 2 * i) * 16;
        xy[2 * i + 1] = (int32_t)((uni ? 156.0f - (w + 1.0f) * 0.5f * amp * 104.0f : 100.0f - w * amp * 52.0f) * 16.0f);
    }
    cv_poly(xy, 109, 32, T_THEME, 3u);
}
static void pic_matrix(const ui_vm_t *v)
{
    char n[2] = {0, 0};
    for (int32_t k = 0; k < 4; k++) {
        int32_t y = 36 + 32 * k, cur = v->pv[4] == k, off = str_eq(v->ps[k], "OFF"), amt = v->pv[k];
        uint16_t ink = off ? T_DIM : cur || v->pv[4] < 0 ? T_TEXT : T_MID;
        n[0] = (char)('1' + k);
        cv_text(10, y + 2, TY_HD, n, cur ? T_THEME : T_DIM);
        cv_text_fit(30, y, TY_RW, v->ps[k], ink, T_BG, 200);
        cv_rect(30, y + 24, 200, 2, T_LINE);
        cv_rect(129, y + 21, 1, 8, T_FILL);
        if (amt > 0) cv_rect(130, y + 24, amt * 100 / 64, 2, cur ? T_THEME : ink);
        if (amt < 0) cv_rect(130 + amt * 100 / 64, y + 24, -amt * 100 / 64, 2, cur ? T_THEME : ink);
    }
}
static void pic_arp_draw(const ui_vm_t *v)
{
    int32_t span = v->pv[16] ? v->pv[16] : 1, ph = v->pv[17], w = v->pv[18] * 13 / 128, sw = v->pv[19] * 13 / 250;
    if (w < 2) w = 2;
    if (ph >= 0)
        cv_rect(16 + 13 * ph, 50, 13, 112, T_LINE);
    for (int32_t i = 0; i < 16; i++) {
        int32_t y = v->pv[16] ? 150 - v->pv[i] * 94 / span : 104, x = 17 + 13 * i + ((i & 1) ? sw : 0);
        cv_rect(x, y, w > 12 ? 12 : w, 7, !v->pv[20] ? T_OFF : i == ph ? T_THEME : T_MID);
    }
    for (int32_t i = 0; i <= 16; i += 4)
        cv_rect(16 + 13 * i, 166, 1, 3, T_OFF);
    cv_text_r(230, 32, TY_TX, v->ps[0], T_MID, T_BG);
}
static void pic_scale(const ui_vm_t *v)
{
    const int32_t cx = 120, cy = 92, r = 50;
    int32_t s = clamp(v->pv[0], 0, (int32_t)SCALE_TOTAL - 1), on = v->pv[2] != 0, i, n;
    char root[4];
    cv_arc(cx * 16, cy * 16, r * 16, 16, 0, 360, T_LINE);
    if (s < (int32_t)SCALE_LEGACY) {                       /* twelve semitones, the scale's lit */
        uint32_t mask = SCALE_MASK[s];
        for (i = 0; i < 12; i++) {
            float a = (float)i * (FM_TWO_PI / 12.0f) - FM_PI * 0.5f;
            int32_t x = cx + (int32_t)(fm_cosf(a) * (float)r), y = cy + (int32_t)(fm_sinf(a) * (float)r);
            if (!i) cv_disc(x, y, 11, on ? T_THEME : T_MID, 0);
            else if ((mask >> i) & 1u) cv_disc(x, y, 7, on ? T_TEXT : T_MID, 0);
            else cv_disc(x, y, 5, T_OFF, 0);
        }
    } else {                                                /* a tuning: its degrees where their pitches lie */
        const micro_scale_t *m = &MICRO_SCALE[s - (int32_t)SCALE_LEGACY];
        n = m->count;
        for (i = 0; i < n; i++) {
            float a = (float)m->pitch[i] / (float)m->pitch[n] * FM_TWO_PI - FM_PI * 0.5f;
            int32_t x = cx + (int32_t)(fm_cosf(a) * (float)r), y = cy + (int32_t)(fm_sinf(a) * (float)r);
            cv_disc(x, y, i ? (n > 24 ? 4 : 6) : 11, i ? (on ? T_TEXT : T_MID) : on ? T_THEME : T_MID, 0);
        }
    }
    str_cpy(root, N_NOTE[clamp(v->pv[1], 0, 11)], sizeof root);
    cv_text_c(cx, ty_y(cy, TY_NM), TY_NM, root, on ? T_TEXT : T_MID, T_BG);
    cv_text_c(cx, 150, TY_TX, SCALE_TITLE[s], T_MID, T_BG);
}
static void pic_chord(const ui_vm_t *v)
{
    int32_t base = v->pv[5], white = 0, off = str_eq(v->ps[0], "OFF");
    const aafont_t *f = text_has(TY_HR, v->ps[0]) ? TY_HR : TY_NM;
    cv_text_c(120, 44, f, v->ps[0], off ? T_DIM : T_TEXT, T_BG);
    for (uint32_t pass = 0; pass < 2u; pass++) {          /* two octaves from base (a C): the chord's notes lit */
        white = 0;
        for (int32_t k = 0; k < 24; k++) {
            int32_t s = k % 12, black = s == 1 || s == 3 || s == 6 || s == 8 || s == 10, lit = 0;
            for (int32_t i = 0; i < v->pv[4] && i < 4; i++) lit |= v->pv[i] == base + k;
            if (!black) {
                if (!pass) cv_rect(22 + 14 * white, 116, 13, 44, lit ? T_THEME : T_WELL);
                white++;
            } else if (pass) {
                int32_t x = 22 + 14 * white - 5;
                cv_rect(x - 2, 116, 13, 28, T_BG);
                cv_rect(x, 116, 9, 26, lit ? T_THEME : T_LINE);
            }
        }
    }
}
static void pic_voices(const ui_vm_t *v)
{
    int32_t cap = v->pv[0] > 0 ? v->pv[0] : 1, w = (216 - 3 * (cap - 1)) / cap, x, i;
    char b[24];
    if (w > 22) w = 22;
    x = 120 - (cap * w + 3 * (cap - 1)) / 2;
    for (i = 0; i < cap; i++)
        cv_rect(x + i * (w + 3), 66, w, 44, (v->pv[2] >> i) & 1 ? T_THEME : T_WELL);
    fmt_int(b, cap);
    str_cpy(b + str_len(b), cap == 1 ? " VOICE" : " VOICES", 12);
    cv_text_c(120, 124, TY_TX, b, T_MID, T_BG);
}
static void pic_faders(const ui_vm_t *v)                 /* each column's value, tall over its column */
{
    for (uint32_t i = 0; i < 4u; i++) {
        const vm_col_t *c = &v->col[i];
        int32_t x = UI_COL_X(i), top = 40, h = 124, k;
        uint16_t ink = i == v->hot ? T_THEME : c->dim ? T_LINE : T_FILL;
        if (!c->on)
            continue;
        switch (c->bar) {
        case BAR_FILL:
            cv_rect(x + 18, top, 14, h, T_WELL);
            cv_rect(x + 18, top + h - c->b * h / 50, 14, c->b * h / 50, ink);
            break;
        case BAR_BIPOLAR: {
            int32_t mid = top + h / 2;                  /* (a..b around 25: below the middle, or above it) */
            cv_rect(x + 18, top, 14, h, T_WELL);
            if (c->a < 25) cv_rect(x + 18, mid, 14, (25 - c->a) * h / 50, ink);
            else cv_rect(x + 18, mid - (c->b - 25) * h / 50, 14, (c->b - 25) * h / 50, ink);
            cv_rect(x + 12, mid, 26, 1, T_MID);
            break;
        }
        case BAR_TICK:
            cv_rect(x + 24, top, 2, h, T_WELL);
            cv_rect(x + 12, top + h - 6 - c->a * (h - 6) / 44, 26, 6, ink);
            break;
        case BAR_STEPS:                                   /* a switch or a short list: one block a value */
            for (k = 0; k < c->b; k++) {
                int32_t bh = (h - 4 * (c->b - 1)) / c->b, y = top + h - (k + 1) * bh - k * 4;
                cv_rect(x + 14, y, 22, bh, k == c->a ? ink : T_WELL);
            }
            break;
        default: break;
        }
    }
}
static void pic_filter(const ui_vm_t *v)                 /* a low pass: flat, its peak at the cut, falling past it */
{
    int32_t xy[2 * 55], i, fc = 20 + v->pv[0] * 190 / 127;
    float q = (float)v->pv[1] / 127.0f;
    for (i = 0; i < 55; i++) {
        int32_t x = 12 + 4 * i;
        float d = (float)(x - fc) / 24.0f, y;
        y = q * 48.0f * fm_expf(-d * d * 2.0f) - (d > 0.0f ? d * 26.0f : 0.0f);   /* (the peak, then the slope) */
        if (y < -48.0f) y = -48.0f;                       /* (the floor of the picture) */
        xy[2 * i] = x * 16;
        xy[2 * i + 1] = (int32_t)((120.0f - y) * 16.0f);
    }
    cv_rect(12, 120, 216, 1, T_LINE);
    cv_rect(fc, 50, 1, 120, T_LINE);
    cv_poly(xy, 55, 32, T_THEME, 3u);
}
/* FM6's algorithm as the DX7 draws it: the carriers on the floor, each modulator over what it modulates (fm6_core.c
 * FM6_ALG: the operators run sixth first, each reads a bus and writes one); the operator of the page lit, those
 * switched off dim, a loop on the one that feeds back */
static void pic_algo(const ui_vm_t *v)
{
    uint32_t a = (uint32_t)v->pv[0] & 31u, bus[3] = {0, 0, 0}, mods[6] = {0}, k, o;
    int32_t col[6], row[6], owner[6], next = 0, ncol, nrow = 1, cw, rh, x[6], y[6];
    uint32_t stack[12], sp = 0, placed = 0, carriers = 0;
    char n[2] = {0, 0};
    for (k = 0; k < 6u; k++) {                          /* (k: run order, OP 6 - k) */
        uint32_t f = FM6_ALG[a][k], op = 5u - k;
        if (f & 0x30u) mods[op] = bus[(f >> 4) & 3u];
        if (f & 3u) bus[f & 3u] = (f & 4u ? bus[f & 3u] : 0u) | 1u << op;
        else carriers |= 1u << op;
    }
    for (o = 0; o < 6u; o++) col[o] = row[o] = owner[o] = -1;
    for (o = 0; o < 6u; o++) {                          /* depth first from each carrier: a leaf takes the next column */
        if (!((carriers >> o) & 1u)) continue;
        row[o] = 0;
        stack[sp++] = o;
        while (sp) {
            uint32_t c = stack[sp - 1u], m, any = 0;
            for (m = 0; m < 6u; m++)
                if ((mods[c] >> m) & 1u && !((placed >> m) & 1u) && row[m] < 0) {
                    row[m] = row[c] + 1;
                    owner[m] = (int32_t)c;
                    if (row[m] + 1 > nrow) nrow = row[m] + 1;
                    stack[sp++] = m;
                    any = 1;
                    break;
                }
            if (any) continue;
            sp--;
            {                                           /* its column: the middle of the modulators it brought, or the
                                                         * next free (one it shares stays over its first) */
                int32_t s = 0, cnt = 0;
                for (m = 0; m < 6u; m++)
                    if ((mods[c] >> m) & 1u && owner[m] == (int32_t)c) { s += col[m]; cnt++; }
                col[c] = cnt ? (s + cnt / 2) / cnt : next++;
                if (col[c] >= next) next = col[c] + 1;
            }
            placed |= 1u << c;
        }
    }
    ncol = next > 0 ? next : 1;
    cw = 216 / ncol; if (cw > 44) cw = 44;
    rh = 128 / nrow; if (rh > 36) rh = 36;
    for (o = 0; o < 6u; o++) {
        x[o] = 120 - ncol * cw / 2 + col[o] * cw + cw / 2;
        y[o] = 156 - row[o] * rh;
    }
    cv_rect(12, 168, 216, 1, T_LINE);                   /* the output */
    for (o = 0; o < 6u; o++) {                          /* the wires: modulator top to bottom, carrier down to the floor */
        for (k = 0; k < 6u; k++)
            if ((mods[o] >> k) & 1u) {
                int32_t xy[4] = {x[k] * 16, (y[k] + 9) * 16, x[o] * 16, (y[o] - 9) * 16};
                cv_poly(xy, 2, 24, T_MID, 3u);
            }
        if ((carriers >> o) & 1u)
            cv_rect(x[o], y[o] + 9, 1, 168 - y[o] - 9, T_MID);
    }
    for (o = 0; o < 6u; o++) {
        uint32_t f = FM6_ALG[a][5u - o], on = (v->pv[2] >> (5u - o)) & 1u, cur = (int32_t)o == v->pv[1];
        uint16_t box = cur ? T_THEME : on ? T_FILL : T_WELL, ink = cur ? T_BG : on ? T_TEXT : T_DIM;
        if (f & 0xC0u) {                                /* feedback: a loop from its top round its right side */
            int32_t xy[8] = {x[o] * 16, (y[o] - 9) * 16, x[o] * 16, (y[o] - 14) * 16, (x[o] + 15) * 16, (y[o] - 14) * 16,
                             (x[o] + 15) * 16, y[o] * 16};
            cv_poly(xy, 4, 20, v->pv[3] ? T_MID : T_LINE, 3u);
        }
        cv_rect(x[o] - 11, y[o] - 9, 22, 18, box);
        n[0] = (char)('1' + o);
        cv_text_c(x[o], y[o] - 8, TY_HD, n, ink, box);
    }
}
static void pic_fmeg(const ui_vm_t *v)                  /* a DX7 envelope: from L4 at R1 to L1, R2 L2, R3 L3, held, R4 L4 */
{
    int32_t xy[12], x = 14, i, lv[5];
    lv[0] = v->pv[7];
    for (i = 0; i < 4; i++) lv[i + 1] = v->pv[4 + i];
    xy[0] = x * 16; xy[1] = (160 - lv[0] * 112 / 99) * 16;
    for (i = 0; i < 4; i++) {
        x += 8 + (99 - clamp(v->pv[i], 0, 99)) * 34 / 99 + (i == 3 ? 28 : 0);   /* (the hold before R4) */
        if (i == 3) { xy[8] = (x - 28) * 16; xy[9] = xy[7]; }
        xy[2 * (i + 1) + (i == 3 ? 2 : 0)] = x * 16;
        xy[2 * (i + 1) + 1 + (i == 3 ? 2 : 0)] = (160 - lv[i + 1] * 112 / 99) * 16;
    }
    cv_rect(12, 161, 216, 1, T_LINE);
    if (v->pv[8]) cv_rect(12, 160 - 50 * 112 / 99, 216, 1, T_LINE);   /* (the pitch EG: 50 is the note itself) */
    cv_poly(xy, 6, 32, T_THEME, 3u);
    for (i = 1; i < 6; i++)
        if (i != 4) cv_disc(xy[2 * i] / 16, xy[2 * i + 1] / 16, 6, T_THEME, 0);
}
static void pic_czeg(const ui_vm_t *v)                  /* a CZ-1 envelope: steps 1..END from 0, SUS ringed */
{
    int32_t xy[2 * 10], x = 14, end = clamp(v->pv[17], 0, 7), sus = v->pv[16], i, n = 1;
    xy[0] = x * 16; xy[1] = 160 * 16;
    for (i = 0; i <= end; i++) {
        x += 6 + (99 - clamp(v->pv[i], 0, 99)) * (180 / (end + 1)) / 99;
        xy[2 * n] = x * 16;
        xy[2 * n + 1] = (160 - (i == end ? 0 : v->pv[8 + i]) * 112 / 99) * 16;
        n++;
    }
    cv_rect(12, 161, 216, 1, T_LINE);
    cv_poly(xy, n, 32, T_THEME, 3u);
    for (i = 1; i < n; i++) {
        cv_disc(xy[2 * i] / 16, xy[2 * i + 1] / 16, 6, T_THEME, 0);
        if (i - 1 == sus && sus < end) cv_ring(xy[2 * i] / 16, xy[2 * i + 1] / 16, 14, T_TEXT);
    }
}
static void pic_readout(const ui_vm_t *v)                 /* the first value, large; its unit under it */
{
    const aafont_t *f = text_has(TY_PO, v->col[0].val) && text_w(TY_PO, v->col[0].val) <= 220 ? TY_PO : TY_HR;
    if (!v->col[0].on)
        return;
    cv_text_c(120, ty_y(100, f), f, v->col[0].val, T_TEXT, T_BG);
    if (v->col[0].unit[0])
        cv_text_c(120, 150, TY_TX, v->col[0].unit, T_MID, T_BG);
}
static void pic_draw(const ui_vm_t *v)
{
    switch (v->pic) {
    case PIC_BTDEV:
        cv_text_c(120,40,TY_HD,v->ps[0],T_TEXT,T_BG);
        if(v->pv[2]>=2&&v->pv[2]<=4){
            if(v->ps[2][0]){
                cv_text_c(120,77,TY_NM,v->ps[1],T_TEXT,T_BG);
                cv_text_c(120,122,TY_TX,v->ps[2],T_MID,T_BG);
            }else cv_text_fit(10,88,TY_RW,v->ps[1],T_TEXT,T_BG,220);
        }else for(int i=0;i<3;i++){
            int y=73+24*i,on=v->pv[0]==v->pv[3]+i;
            if(!v->ps[i+1][0])continue;
            if(on)cv_rect(6,y-2,228,21,T_FILL);
            cv_text_fit(10,y,TY_TX,v->ps[i+1],on?T_TEXT:T_MID,on?T_FILL:T_BG,197);
            int bars=v->pv[4+i]>-60?3:v->pv[4+i]>-75?2:1;
            for(int k=0;k<3;k++)cv_rect(215+4*k,y+10-3*k,3,3+3*k,k<bars?T_MID:T_LINE);
        }
        cv_text_c(120,153,TY_LB,v->line[0],T_MID,T_BG);
        break;
    case PIC_BLE:
        cv_text_c(120, 46, TY_HD, v->ps[1], T_MID, T_BG);
        cv_text_c(120, 87, TY_HD, v->ps[0], T_TEXT, T_BG);
        cv_text_c(120, 129, TY_TX, v->ps[2], T_MID, T_BG);
        cv_text_c(120, 149, TY_TX, v->ps[3], T_MID, T_BG);
        break;
    case PIC_ENV: pic_env(v); break;
    case PIC_LFO: pic_lfo(v); break;
    case PIC_MATRIX: pic_matrix(v); break;
    case PIC_ARP: pic_arp_draw(v); break;
    case PIC_SCALE: pic_scale(v); break;
    case PIC_CHORD: pic_chord(v); break;
    case PIC_VOICES: pic_voices(v); break;
    case PIC_FADERS: pic_faders(v); break;
    case PIC_P5ENV: pic_env(v); break;
    case PIC_FILTER: pic_filter(v); break;
    case PIC_ALGO: pic_algo(v); break;
    case PIC_FMEG: pic_fmeg(v); break;
    case PIC_CZEG: pic_czeg(v); break;
    case PIC_READOUT: pic_readout(v); break;
    case PIC_HYSLOTS: case PIC_HYWAVE: case PIC_HYSPEC: case PIC_HYNAME: hylab_draw(v); break;
    }
}
