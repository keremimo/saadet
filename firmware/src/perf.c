/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Performances: named two-part setups. A slot keeps the parts mode, the split point, part B's channel, the part selected
 * and both parts' whole sounds, so it plays the same after the user slots it came from changed. It holds no tempo or MIDI
 * IN / OUT: those are the instrument's. Two slots share an A/B flash object (storage.c OBJ_PERF0..): a slot's write rewrites
 * its neighbour's bytes as read, and a torn write leaves the previous copy in charge. A RAM index keeps the names, so the
 * library's lists read no flash. Included by persist.c. */
#define PERF_OBJS 6u
#define PERF_PER_OBJ 2u
_Static_assert(PERF_SLOTS == PERF_OBJS * PERF_PER_OBJ, "the slots the library counts");
#define PERF_MAGIC 0x31465053u                    /* "SPF1" */
typedef struct {
    uint8_t used, mode, split, ch2, sel;
    char name[PERF_NAME];
    synth_part_t part[NPART];
} perf_rec_t;
typedef struct { uint32_t magic, rec_size; perf_rec_t r[PERF_PER_OBJ]; } perf_bank_t;
/* a bank from before HYDRA: its parts have no patch */
typedef struct {
    uint8_t used, mode, split, ch2, sel;
    char name[PERF_NAME];
    synth_part2_t part[NPART];
} perf_rec2_t;
typedef struct { uint32_t magic, rec_size; perf_rec2_t r[PERF_PER_OBJ]; } perf_bank2_t;
_Static_assert(sizeof(perf_bank_t) <= ST_PAYLOAD_MAX - 32u, "two performances fit their flash object");
static perf_bank_t perf_buf __attribute__((section(".pool")));
#if MELODEE_FLASH
_Static_assert(OBJ_HYWAVE0 - OBJ_PERF0 == PERF_OBJS, "a flash object per pair of performances");
static int perf_obj_load(uint32_t obj) { return st_load(OBJ_PERF0 + obj, &perf_buf, sizeof perf_buf); }
static int perf_obj_save(uint32_t obj) { return st_save(OBJ_PERF0 + obj, &perf_buf, sizeof perf_buf); }
#else                                             /* (the host's interface tests: no flash) */
static perf_bank_t perf_ram[PERF_OBJS];
static uint8_t perf_ram_used[PERF_OBJS];
static int perf_obj_load(uint32_t obj)
{
    if (!perf_ram_used[obj]) return 0;
    perf_buf = perf_ram[obj];
    return (int)sizeof perf_buf;
}
static int perf_obj_save(uint32_t obj)
{
    perf_ram[obj] = perf_buf;
    perf_ram_used[obj] = 1;
    return 0;
}
#endif
static struct { uint8_t used, eng; char name[PERF_NAME]; } perf_index[PERF_SLOTS];
static uint8_t perf_now;                          /* the slot + 1 last loaded or stored, 0: none */

static int perf_rec_valid(const perf_rec_t *r)
{
    uint32_t i;
    if (r->used != 1u || r->mode >= PT_N || r->split > 127u || r->ch2 < 1u || r->ch2 > 16u || r->sel >= NPART)
        return 0;
    for (i = 0; i < PERF_NAME && r->name[i]; i++)
        ;
    if (i == PERF_NAME)                           /* (no end to the name) */
        return 0;
    for (i = 0; i < NPART; i++)
        if (!part_valid(&r->part[i]))
            return 0;
    return 1;
}
/* a bank of *len stored bytes from before HYDRA -> today's, in place (b holds a whole bank), each part with HYDRA's INIT
 * patch. Any other bank stays as it is, for perf_bank_valid to judge */
static void perf_bank_upgrade(perf_bank_t *b, uint32_t *len)
{
    const perf_bank2_t *o = (const perf_bank2_t *)b;
    if (*len != sizeof *o || o->magic != PERF_MAGIC || o->rec_size != sizeof(perf_rec2_t))
        return;
    for (uint32_t i = PERF_PER_OBJ; i--;) {          /* (from the last back: nothing is overwritten before it moved) */
        parts_upgrade(b->r[i].part, (const uint8_t *)o->r[i].part, NPART);
        bytes_up(&b->r[i], &o->r[i], __builtin_offsetof(perf_rec2_t, part));
    }
    b->rec_size = sizeof(perf_rec_t);
    *len = sizeof *b;
}
/* a stored bank: empty (len 0), or every slot in use valid. Backups and flash both pass here */
static int perf_bank_valid(const perf_bank_t *b, uint32_t len)
{
    uint32_t i;
    if (!len)
        return 1;
    if (len != sizeof *b || b->magic != PERF_MAGIC || b->rec_size != sizeof(perf_rec_t))
        return 0;
    for (i = 0; i < PERF_PER_OBJ; i++)
        if (b->r[i].used && !perf_rec_valid(&b->r[i]))
            return 0;
    return 1;
}
/* object obj into perf_buf, or a blank one when the flash holds none. 0: read or empty; -1: unreadable, left blank */
static int perf_bank_load(uint32_t obj)
{
    int n = perf_obj_load(obj);
    uint32_t len = n > 0 ? (uint32_t)n : 0u;
    perf_bank_upgrade(&perf_buf, &len);
    if (n <= 0 || !perf_bank_valid(&perf_buf, len)) {
        memset(&perf_buf, 0, sizeof perf_buf);
        perf_buf.magic = PERF_MAGIC;
        perf_buf.rec_size = sizeof(perf_rec_t);
        return n > 0 ? -1 : 0;
    }
    return 0;
}
static void perf_index_obj(uint32_t obj)          /* perf_buf is object obj */
{
    uint32_t i;
    for (i = 0; i < PERF_PER_OBJ; i++) {
        uint32_t k = obj * PERF_PER_OBJ + i;
        const perf_rec_t *r = &perf_buf.r[i];
        perf_index[k].used = r->used;
        perf_index[k].eng = r->used ? r->part[0].engine : 0u;
        memcpy(perf_index[k].name, r->used ? r->name : "", r->used ? PERF_NAME : 1u);
    }
}
static void perf_boot(void)
{
    uint32_t obj;
    memset(perf_index, 0, sizeof perf_index);
    perf_now = 0;
    for (obj = 0; obj < PERF_OBJS; obj++) {
        perf_bank_load(obj);
        perf_index_obj(obj);
    }
}
static int perf_used(uint32_t k) { return k < PERF_SLOTS && perf_index[k].used; }
static uint32_t perf_engine(uint32_t k) { return k < PERF_SLOTS ? perf_index[k].eng : 0u; }
static void perf_name(uint32_t k, char *b) { str_cpy(b, k < PERF_SLOTS ? perf_index[k].name : "", PERF_NAME); }
static void perf_label(char *b, uint32_t k)       /* "P01" */
{
    b[0] = 'P'; b[1] = (char)('0' + (k + 1u) / 10u % 10u); b[2] = (char)('0' + (k + 1u) % 10u); b[3] = 0;
}
/* the setup as it stands into slot k under name. 0: stored, 2: refused, the transport runs, else a flash error */
static int perf_store(uint32_t k, const char *name)
{
    uint32_t obj = k / PERF_PER_OBJ;
    perf_rec_t *r = &perf_buf.r[k % PERF_PER_OBJ];
    uint32_t i;
    int rc;
    if (k >= PERF_SLOTS)
        return 1;
    if (transport_busy())
        return 2;
    perf_bank_load(obj);
    memset(r, 0, sizeof *r);
    r->used = 1;
    r->mode = (uint8_t)parts_mode();
    r->split = (uint8_t)song.g[G_PSPLIT];
    r->ch2 = (uint8_t)song.g[G_PCH2];
    r->sel = song.sel;
    str_cpy(r->name, name, PERF_NAME);
    for (i = 0; i < NPART; i++)
        part_capture(i, &r->part[i]);
    rc = perf_obj_save(obj);
    if (rc)
        return rc;
    perf_index_obj(obj);
    perf_now = (uint8_t)(k + 1u);
    up_gen++;                                     /* (a backup in progress starts again; the lists read anew) */
    return 0;
}
/* slot k becomes the setup: everything sounding ends, both parts take their sounds. 0: loaded */
static int perf_load(uint32_t k)
{
    uint32_t i;
    const perf_rec_t *r = &perf_buf.r[k % PERF_PER_OBJ];
    if (!perf_used(k) || perf_bank_load(k / PERF_PER_OBJ) || !r->used)
        return 1;
    panic_req |= (uint8_t)((1u << NTRK) - 1u);
    fm1_irq_off();
    song.g[G_PMODE] = r->mode;
    song.g[G_PSPLIT] = r->split;
    song.g[G_PCH2] = r->ch2;
    fm1_irq_on();
    for (i = 0; i < NPART; i++)
        part_apply(i, &r->part[i]);
    scale_share(&trk[r->sel]);
    song.sel = r->mode == PT_ONE ? 0u : r->sel;
    perf_now = (uint8_t)(k + 1u);
    sync_reload = 1;
    ui.force = 1;
    return 0;
}
/* slot k's name. 0: renamed, 1: no such performance, 2: refused, the transport runs, else a flash error */
static int perf_rename(uint32_t k, const char *name)
{
    uint32_t obj = k / PERF_PER_OBJ;
    perf_rec_t *r = &perf_buf.r[k % PERF_PER_OBJ];
    int rc;
    if (!perf_used(k))
        return 1;
    if (transport_busy())
        return 2;
    if (perf_bank_load(obj) || !r->used)
        return 1;
    memset(r->name, 0, PERF_NAME);
    str_cpy(r->name, name, PERF_NAME);
    rc = perf_obj_save(obj);
    if (rc)
        return rc;
    perf_index_obj(obj);
    up_gen++;
    return 0;
}
static int perf_delete(uint32_t k)                /* 0: gone */
{
    uint32_t obj = k / PERF_PER_OBJ;
    int rc;
    if (!perf_used(k))
        return 1;
    if (transport_busy())
        return 2;
    perf_bank_load(obj);
    memset(&perf_buf.r[k % PERF_PER_OBJ], 0, sizeof(perf_rec_t));
    rc = perf_obj_save(obj);
    if (rc)
        return rc;
    perf_index_obj(obj);
    if (perf_now == k + 1u)
        perf_now = 0;
    up_gen++;
    return 0;
}
