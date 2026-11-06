/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Complete musical archive: no raw addresses are accepted. Objects are staged
 * in main-loop RAM and validated before the existing A/B commit path writes.
 * Only settings, shared sounds and the current synth state are supported.
 * Project, pattern, song and sample object identities are rejected.
 */
#ifndef ED_BK_FLASH_PTR
#define ED_BK_FLASH_PTR(off) fm1_xip_ptr(off)
#endif
/* Object 32 is Saadet state; 28 remains the final unified preset bank. */
#define ED_BK_PERF0 33u                            /* 33..: the performances' flash objects */
#define ED_BK_HYW0 39u                             /* 39, 40: HYDRA's user waves (hydra_bank.c) */
static const uint8_t ED_BK_IDS[] = {1,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,32,33,34,35,36,37,38,39,40};
_Static_assert(ED_BK_PERF0 + PERF_OBJS - 1u == 38u && ED_BK_HYW0 == 39u, "a backup id per performance object, then the waves'");
#define ED_BK_N ((uint32_t)sizeof ED_BK_IDS)
#define ED_BK_RAW transfer_scratch.raw
#define ED_BK_MAX ((uint32_t)sizeof transfer_scratch.raw)
static synth_state_t ed_bk_synth __attribute__((section(".pool")));
#define ed_bk_set set_rec
static uint32_t ed_bk_up_gen, ed_bk_settings_crc;
static uint32_t ed_bk_set_len;
static uint8_t ed_bk_valid, ed_bk_put, ed_bk_id;
static uint32_t ed_bk_gen;
static uint32_t ed_bk_len, ed_bk_crc, ed_bk_pos, ed_bk_ms, ed_bk_usb;
#if MELODEE_FLASH
static uint32_t ed_bk_source_obj, ed_bk_source_copy, ed_bk_source_crc;
#endif
static void ed_bk_u32(uint32_t n) { for (uint32_t i = 0; i < 5u; i++) ed_b((n >> (i * 7u)) & 127u); }
static uint32_t ed_bk_r32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28;
}
static void ed_bk_pack(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, mask = 0;
        for (uint32_t i = 0; i < k; i++) mask |= (uint32_t)(p[i] >> 7) << i;
        ed_b(mask);
        for (uint32_t i = 0; i < k; i++) ed_b(p[i]);
        p += k; n -= k;
    }
}
static uint32_t ed_bk_preset_bank(uint32_t id)
{
    if(id==6u || id==7u)return id-6u;
    if(id==17u || id==18u)return id-15u;
    if(id>=19u && id<=22u)return id<21u?id-13u:id-17u;
    if(id>=9u && id<=16u)return id-1u;
    if(id>=23u && id<=28u)return id-7u;
    return UP_BANKS;
}
static int ed_bk_supported(uint32_t id)
{ for(uint32_t i=0;i<ED_BK_N;i++)if(ED_BK_IDS[i]==id)return 1;return 0; }
static const uint8_t *ed_bk_object(uint32_t id,uint32_t *len)
{
    *len=0;
    if(!ed_bk_supported(id))return 0;
#if MELODEE_FLASH
    ed_bk_source_obj=OBJ_COUNT;
#endif
    if(id==32u){*len=sizeof ed_bk_synth;return (const uint8_t *)&ed_bk_synth;}
    if(id==1u){*len=ed_bk_set_len;return (const uint8_t *)&ed_bk_set;}
    uint32_t bank=ed_bk_preset_bank(id);
#if MELODEE_FLASH
    if(bank<UP_BANKS || id>=ED_BK_PERF0){
        uint32_t obj=id>=ED_BK_HYW0?OBJ_HYWAVE0+id-ED_BK_HYW0:id>=ED_BK_PERF0?OBJ_PERF0+id-ED_BK_PERF0:up_obj(bank);st_hdr_t h;int copy=st_current(obj,&h);
        if(copy>=0){*len=h.len;ed_bk_source_obj=obj;ed_bk_source_copy=(uint32_t)copy;ed_bk_source_crc=h.crc;
            return ED_BK_FLASH_PTR(st_sector(obj,(uint32_t)copy)+st_payload_off_header(obj,&h));}
    }
#else
    if(bank<UP_BANKS){*len=sizeof(up_bank_t);return (const uint8_t *)up_cache_bank(bank);}
#endif
    return ED_BK_RAW;
}
static uint32_t ed_bk_capture(void)
{
    if(ed_flash_stop())return 3;
    if(hy_bank_store())return 4;                   /* (the user waves as RAM has them: the archive reads flash) */
    state_capture(&ed_bk_synth);
#if MELODEE_FLASH
    ed_bk_set.p=persist_saved;
#endif
    settings_export(&ed_bk_set.p);
    ed_bk_set_len=sizeof ed_bk_set.p+tmpl_len;
    ed_bk_settings_crc=st_crc32(&ed_bk_set,ed_bk_set_len);
    ed_bk_gen=transfer_gen;ed_bk_up_gen=up_gen;
    ed_bk_put=0;ed_bk_valid=1;ed_bk_usb=usb.resets;return 0;
}
static int ed_bk_panel_valid(const panel_t *p)
{
    uint32_t b = 0, e = 0;
    if (p->magic != PANEL_MAGIC) return 0;
    for (uint32_t i = 0; i < NB; i++) {
        if (p->btn[i] >= NB || (b & (1u << p->btn[i]))) return 0;
        b |= 1u << p->btn[i];
    }
    for (uint32_t i = 0; i < NE; i++) {
        if (p->enc[i] >= NE || (e & (1u << p->enc[i])) || (p->dir[i] != 1 && p->dir[i] != -1)) return 0;
        e |= 1u << p->enc[i];
    }
    return 1;
}
static uint32_t ed_bk_commit_inner(void)
{
    uint8_t *raw=ED_BK_RAW;
    if(ed_bk_pos!=ed_bk_len || st_crc32(raw,ed_bk_len)!=ed_bk_crc)return 2;
    uint32_t bank=ed_bk_preset_bank(ed_bk_id);
#if MELODEE_FLASH
    if(!flash_ok)return 4;
    if(ed_bk_id==32u){
        /* (a backup from before JUNO-106 restores with the INIT tone, one from before HYDRA with its INIT patch) */
        if(!state_upgrade((synth_state_t *)raw,ed_bk_len) || !state_valid((const synth_state_t *)raw))return 2;
        if(st_save(OBJ_SYNTH,raw,sizeof(synth_state_t)))return 4;
        syn_saved=0;syn_retry=0;state_boot();return 0;
    }
    if(ed_bk_id>=ED_BK_HYW0){
        uint32_t j=ed_bk_id-ED_BK_HYW0;
        if(ed_bk_len && !hy_uobj_valid((const hy_uobj_t *)(const void *)raw,j))return 2;
        if(st_save(OBJ_HYWAVE0+j,raw,ed_bk_len))return 4;
        hy_bank_boot();return 0;                       /* (an empty one: free waves) */
    }
    if(ed_bk_id>=ED_BK_PERF0){
        uint32_t len=ed_bk_len;
        perf_bank_upgrade((perf_bank_t *)raw,&len);
        if(!perf_bank_valid((const perf_bank_t *)raw,len))return 2;
        if(st_save(OBJ_PERF0+ed_bk_id-ED_BK_PERF0,raw,len))return 4;
        perf_boot();up_gen++;return 0;
    }
    if(ed_bk_id==1u){
        persist_t p;
        if(ed_bk_len<sizeof p || ed_bk_len>sizeof set_rec)return 2;
        memcpy(&p,raw,sizeof p);
        if(!ed_bk_panel_valid(&p.panel) || p.magic!=PERSIST_MAGIC || !settings_legacy_valid(&p))return 2;
        uint32_t tail=ed_bk_len-sizeof p;
        if(tail && !tmpl_blob_valid(raw+sizeof p,tail))return 2;
        if(st_save(OBJ_SETTINGS,raw,ed_bk_len))return 4;
        memcpy(&set_rec,raw,ed_bk_len);tmpl_len=tail;
        settings_import(&set_rec.p,sizeof set_rec.p);settings_old_accept();
        persist_saved=set_rec.p;persist_pending=0;panel_init();settings_init();palette_set(settings.palette);return 0;
    }
    if(bank<UP_BANKS){
        if(ed_bk_len){
            if(ed_bk_len!=sizeof(up_disk_bank_t))return 2;
            up_disk_bank_t *p=(up_disk_bank_t *)raw;
            if(p->magic!=UP_DISK_MAGIC || p->rsize!=sizeof(up_disk_rec_t) || p->nslot!=UP_PER_BANK)return 2;
            for(uint32_t j=0;j<UP_PER_BANK;j++){
                up_rec_t r;memset(&r,0,sizeof r);memcpy(&r,p->r[j].head,160u);memcpy(r.cz_extra,p->r[j].extra,46u);
                if(r.used && !up_valid(&r))return 2;
                if(bank*UP_PER_BANK+j>=UP_SLOTS && r.used)return 2;
            }
        }
        if(st_save(up_obj(bank),raw,ed_bk_len))return 4;
        up_cache_reset();up_gen++;sync_reload=1;ui.force=1;return 0;
    }
#endif
    return 1;
}
static uint32_t ed_bk_commit(void) { return ed_bk_commit_inner(); }
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    if (n < 2u || a[0] > 3u || !ed_bk_supported(a[1])) return 1;
    if (ed_flash_stop()) return 3;
    if (a[0] == 0u) {
        if (n != 12u || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if(len>ED_BK_MAX || (a[1]==32u && len!=sizeof(synth_state_t) && len!=sizeof(synth_state2_t) && len!=sizeof(synth_state1_t) && len!=SYN_SIZE_V1) ||
            (a[1]>=ED_BK_PERF0 && a[1]<ED_BK_HYW0 && len && len!=sizeof(perf_bank_t) && len!=sizeof(perf_bank2_t)) ||
            (a[1]>=ED_BK_HYW0 && len && len!=sizeof(hy_uobj_t)) ||
            (a[1]==1u && (len<sizeof(persist_t) || len>sizeof set_rec)) ||
            (ed_bk_preset_bank(a[1])<UP_BANKS && len && len!=sizeof(up_disk_bank_t)))return 1;
        ed_bk_valid = 0; ed_bk_put = 1; ed_bk_id = a[1]; ed_bk_len = len; ed_bk_gen = ++transfer_gen;
        ed_bk_crc = ed_bk_r32(a + 7); ed_bk_pos = 0;
        ed_bk_usb = usb.resets; ed_bk_ms = fm1_ms;
        return 0;
    }
    if (!ed_bk_put || ed_bk_id != a[1] || ed_bk_usb != usb.resets || fm1_ms - ed_bk_ms > 15000u ||
        ed_bk_gen != transfer_gen) {                /* (another transfer reused the staging RAM) */
        ed_bk_put = 0; return 5;
    }
    ed_bk_ms = fm1_ms;
    if (a[0] == 3u) { ed_bk_put = 0; return n == 2u ? 0u : 1u; }
    if (a[0] == 2u) {
        if (n != 2u) return 1;
        uint32_t rc = ed_bk_commit(); ed_bk_put = 0; return rc;
    }
    if (n < 9u || a[6] > 15u || ed_bk_r32(a + 2) != ed_bk_pos) return 1;
    uint32_t count = ed_unpack7(a + 7, n - 7u, ed_smp_buf, 256u);
    if (!count || count > ed_bk_len - ed_bk_pos) return 1;
    memcpy(ED_BK_RAW + ed_bk_pos, ed_smp_buf, count); ed_bk_pos += count;
    return 0;
}
static int ed_backup_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_BACKUP_LIST) {
        uint32_t rc = n ? 1u : ed_bk_capture();
        ed_b(1); ed_b(rc); ed_b(rc ? 0u : ED_BK_N);
        if (!rc) for (uint32_t i = 0; i < ED_BK_N; i++) {
            uint32_t len;
            const uint8_t *p = ed_bk_object(ED_BK_IDS[i], &len);
            uint32_t crc;
#if MELODEE_FLASH
            if (ed_bk_source_obj < OBJ_COUNT) crc = ed_bk_source_crc;
            else
#endif
                crc = st_crc32(p, len);
            ed_b(ED_BK_IDS[i]); ed_bk_u32(len); ed_bk_u32(crc);
            fm1_wdt_feed();
        }
        return 1;
    }
    if (cmd == ED_BACKUP_GET) {
        uint32_t len = 0, off = n >= 6u ? ed_bk_r32(a + 1) : 0u;
        uint32_t count = n == 8u ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u;
        const uint8_t *p = n ? ed_bk_object(a[0], &len) : 0;
        uint32_t rc = (!ed_bk_valid || ed_bk_usb != usb.resets || (ed_bk_gen!=transfer_gen || ed_bk_up_gen!=up_gen || ed_bk_settings_crc!=st_crc32(&ed_bk_set,ed_bk_set_len))) ? 5u : transport_busy() ? 3u :
            n != 8u || a[5] > 15u || !p || !count || count > 256u || off > len || count > len - off ? 1u : 0u;
        uint32_t data_off = off;
#if MELODEE_FLASH
        if (!rc && ed_bk_source_obj < OBJ_COUNT) {
            if (st_read_range(ed_bk_source_obj, ed_bk_source_copy, off, ed_smp_buf, count)) rc = 4;
            p = ed_smp_buf; data_off = 0;
        }
#endif
        ed_b(n ? a[0] : 127u); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc) ed_bk_pack(p + data_off, count);
        return 1;
    }
    if (cmd == ED_BACKUP_PUT) {
        uint32_t rc = ed_bk_write(a, n);
        ed_b(n ? a[0] : 127u); ed_b(n >= 2u ? a[1] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}
