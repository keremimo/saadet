/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HYDRA's user waves in flash: the bank (hydra_bank.h) is two compact objects, OBJ_HYWAVE0 and +1 (storage.c), of
 * HY_UOBJ waves each, stored as RAM holds them. Boot loads them; an object that is missing or does not read as a bank
 * comes back as free waves. A change (the editor's command 98) is heard at once and marked; the marked objects go to
 * flash when the unit rests (state_autosave: nothing touched for AUTOSAVE_MS, nothing sounding), so a write never
 * silences what plays, or at once on the editor's STORE. Included by persist.c */
#define HY_UMAGIC 0x31575948u                          /* "HYW1" */
static uint32_t hy_udirty;                             /* bit k: U(k+1) changed since flash held it */
#if MELODEE_FLASH
_Static_assert(sizeof(hy_uobj_t) <= ST_SECTOR - 256u - sizeof(st_hdr_t), "a compact object holds seven waves");
_Static_assert(OBJ_COUNT == OBJ_HYWAVE0 + 2u, "two objects hold the bank");
#endif

static void hy_uname_default(char *n, uint32_t k)     /* USER 1 .. USER 14 */
{
    memset(n, 0, HY_UNAME + 1u);
    memcpy(n, "USER ", 5);
    k++;
    if (k >= 10u) {
        n[5] = (char)('0' + k / 10u);
        n[6] = (char)('0' + k % 10u);
    } else
        n[5] = (char)('0' + k);
}
static void hy_uwave_default(hy_uwave_t *w, uint32_t k)   /* free: the factory sine */
{
    memcpy(w->pts, HY_WAVES[HYW_SINE], sizeof w->pts);
    hy_uname_default(w->name, k);
    w->used = 0;
    memset(w->rsv, 0, sizeof w->rsv);
}
static void hy_uobj_default(uint32_t j)
{
    hy_uobj_t *o = &hy_ubank[j];
    o->magic = HY_UMAGIC;
    o->first = (uint8_t)(j * HY_UOBJ);
    o->count = HY_UOBJ;
    o->rsv[0] = o->rsv[1] = 0;
    for (uint32_t k = 0; k < HY_UOBJ; k++)
        hy_uwave_default(&o->w[k], j * HY_UOBJ + k);
}
static int hy_uname_ok(const char *n)                 /* 1..HY_UNAME printable letters, 0-terminated */
{
    uint32_t i;
    for (i = 0; i <= HY_UNAME && n[i]; i++)
        if (n[i] < 32 || n[i] > 126)
            return 0;
    return i >= 1u && i <= HY_UNAME;
}
static int hy_uobj_valid(const hy_uobj_t *o, uint32_t j)
{
    if (o->magic != HY_UMAGIC || o->first != j * HY_UOBJ || o->count != HY_UOBJ)
        return 0;
    for (uint32_t k = 0; k < HY_UOBJ; k++)
        if (o->w[k].used > 1u || !hy_uname_ok(o->w[k].name))
            return 0;
    return 1;
}
/* a wave changed from outside (the editor, the panel): pts 0 keeps its points, name 0 its name. A name is cut to
 * HY_UNAME letters, others than printable ones become spaces, an empty one is USER n. Heard at the next block */
static void hy_uset(uint32_t k, const int16_t *pts, const char *name, uint32_t used)
{
    char n[HY_UNAME + 1u];
    uint32_t i, len = 0;
    if (k >= HY_UWAVES)
        return;
    hy_uwave_t *w = hy_uwave(k);
    memcpy(n, w->name, sizeof n);
    if (name) {
        memset(n, 0, sizeof n);
        for (i = 0; i < HY_UNAME && name[i]; i++)
            n[i] = name[i] >= 32 && name[i] <= 126 ? name[i] : ' ';
        for (len = i; len && n[len - 1u] == ' '; len--)
            n[len - 1u] = 0;
        for (i = 0; i < len && n[i] == ' '; i++)
            ;
        if (i == len)                                   /* (nothing but spaces) */
            hy_uname_default(n, k);
    }
    fm1_irq_off();                                      /* (the audio copies whole waves only) */
    if (pts)
        memcpy(w->pts, pts, sizeof w->pts);
    memcpy(w->name, n, sizeof n);
    w->used = (uint8_t)(used != 0u);
    hy_ugen++;
    fm1_irq_on();
    hy_udirty |= 1u << k;
}
static void hy_uclear(uint32_t k)                     /* free again: the sine, USER n */
{
    hy_uwave_t w;
    if (k >= HY_UWAVES)
        return;
    hy_uwave_default(&w, k);
    hy_uset(k, w.pts, w.name, 0);
}
static int32_t hy_ufree(void)                         /* the first free wave, -1 none */
{
    for (uint32_t k = 0; k < HY_UWAVES; k++)
        if (!hy_uwave(k)->used)
            return (int32_t)k;
    return -1;
}
/* before the audio starts (main.c, after persist_boot) */
static void hy_bank_boot(void)
{
    for (uint32_t j = 0; j < 2u; j++) {
        hy_uobj_default(j);
#if MELODEE_FLASH
        if (flash_ok && (st_load(OBJ_HYWAVE0 + j, &hy_ubank[j], sizeof hy_ubank[j]) != (int)sizeof hy_ubank[j] ||
                         !hy_uobj_valid(&hy_ubank[j], j)))
            hy_uobj_default(j);
#endif
    }
    hy_udirty = 0;
    hy_ugen++;
}
/* U(k+1) as flash holds it again. 0: done */
static int hy_urevert(uint32_t k)
{
    if (k >= HY_UWAVES)
        return -1;
#if MELODEE_FLASH
    {
        hy_uobj_t *o = (hy_uobj_t *)(void *)transfer_borrow();
        uint32_t j = k / HY_UOBJ;
        int n = flash_ok ? st_load(OBJ_HYWAVE0 + j, o, sizeof *o) : -1;
        if (n == (int)sizeof *o && hy_uobj_valid(o, j)) {
            const hy_uwave_t *w = &o->w[k % HY_UOBJ];
            hy_uset(k, w->pts, w->name, w->used);
        } else
            hy_uclear(k);
    }
#else
    hy_uclear(k);
#endif
    hy_udirty &= ~(1u << k);
    return 0;
}
/* the changed objects into flash. 0: flash holds the bank */
static int hy_bank_store(void)
{
#if MELODEE_FLASH
    int rc = 0;
    if (!flash_ok)
        return 0;
    for (uint32_t j = 0; j < 2u; j++) {
        uint32_t bits = ((1u << HY_UOBJ) - 1u) << (j * HY_UOBJ);
        if (!(hy_udirty & bits))
            continue;
        if (st_save(OBJ_HYWAVE0 + j, &hy_ubank[j], sizeof hy_ubank[j]))
            rc = -1;                                    /* (kept marked: tried again at the next rest) */
        else
            hy_udirty &= ~bits;
    }
    return rc;
#else
    hy_udirty = 0;
    return 0;
#endif
}
