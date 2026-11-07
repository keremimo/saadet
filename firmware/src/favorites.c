/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */

#define MELODEE_FAVORITES 1
#define J106_STAR_CAP 224u
#define HY_STAR_CAP 224u                         /* row 2 (PHASE, retired) bytes 0..27, tagged in 28..31 */
#define SCALE_FAV_CAP 96u

typedef struct {
    uint8_t factory[16][32];
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
static uint32_t favorites_user_hi;

static int p5_favorite_has(uint32_t preset, int factory);
static int p5_favorite_set(uint32_t preset, int on, int factory);

/* A tagged row can lend just part of its payload to a newer collection. */
static int star_tag_matches(const uint8_t *row, const uint8_t tag[4])
{
    return !memcmp(row + 28, tag, 4);
}
static int star_bit_get(const uint8_t *bits, uint32_t index)
{
    return !!(bits[index / 8u] & (1u << (index % 8u)));
}
static int star_bit_put(uint8_t *bits, uint32_t index, int on)
{
    uint8_t mask = (uint8_t)(1u << (index % 8u));
    uint8_t *b = bits + index / 8u;
    if (star_bit_get(bits, index) == on) return 0;
    *b = on ? (uint8_t)(*b | mask) : (uint8_t)(*b & ~mask);
    return 1;
}
static int star_tagged_get(const uint8_t *row, uint32_t offset, uint32_t cap,
                           const uint8_t tag[4], uint32_t index)
{
    return index < cap && star_tag_matches(row, tag) && star_bit_get(row + offset, index);
}
static int star_tagged_put(uint8_t *row, uint32_t offset, uint32_t cap,
                           const uint8_t tag[4], uint32_t index, int on)
{
    if (index >= cap || star_tagged_get(row, offset, cap, tag, index) == on) return 0;
    if (!star_tag_matches(row, tag)) {
        memset(row + offset, 0, 28u - offset);
        memcpy(row + 28, tag, 4);
    }
    return star_bit_put(row + offset, index, on);
}
static const uint8_t star_juno_tag[4] = {'J', '6', 'F', 2};
static const uint8_t star_scale_tag[4] = {'S', 'C', 'L', 1};
static const uint8_t star_hydra_tag[4] = {'H', 'Y', 'F', 1};

#if defined(USER_NATIVE_PPG) && defined(SAADET_PPG) && SAADET_PPG
#define STAR_PPG_SPACE(e) ((e) == USER_NATIVE_PPG)   /* (PPG's user namespace, PPG builds) */
#else
#define STAR_PPG_SPACE(e) 0
#endif
static int star_user_namespace(uint32_t engine)
{
    return engine == USER_GENERAL || engine == USER_NATIVE_P5 || engine == USER_NATIVE_FM ||
           engine == USER_NATIVE_CZ || engine == USER_NATIVE_J106 || engine == USER_NATIVE_HYDRA || STAR_PPG_SPACE(engine);
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    if (engine == ENGI_PROPHET) return !!p5_favorite_has(preset, 1);
    if (star_user_namespace(engine)) return preset < UP_SLOTS && !!up_favorite(preset);
    if (engine == ENGI_J106)
        return star_tagged_get(favorites.factory[3], 0, J106_STAR_CAP, star_juno_tag, preset);
    if (engine == ENGI_HYDRA)
        return star_tagged_get(favorites.factory[2], 0, HY_STAR_CAP, star_hydra_tag, preset);
    return engine < NENGINES && engine < 16u && preset < 256u &&
           star_bit_get(favorites.factory[engine], preset);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    on = !!on;
    if (engine == ENGI_PROPHET) return p5_favorite_set(preset, on, 1);
    if (star_user_namespace(engine)) return preset < UP_SLOTS ? up_favorite_set(preset, on) : 0;
    if (engine == ENGI_J106)
        return star_tagged_put(favorites.factory[3], 0, J106_STAR_CAP, star_juno_tag, preset, on);
    if (engine == ENGI_HYDRA)
        return star_tagged_put(favorites.factory[2], 0, HY_STAR_CAP, star_hydra_tag, preset, on);
    return engine < NENGINES && engine < 16u && preset < 256u ?
           star_bit_put(favorites.factory[engine], preset, on) : 0;
}
static int scale_favorite(uint32_t index)
{
    return star_tagged_get(favorites.factory[14], 16, SCALE_FAV_CAP, star_scale_tag, index);
}
static int scale_favorite_set(uint32_t index, int on)
{
    return star_tagged_put(favorites.factory[14], 16, SCALE_FAV_CAP, star_scale_tag, index, !!on);
}
