/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* USB maintenance only: Melodee's radio calibration recovery (its command 81; 81 is the battery's here), for a unit whose
 * stock calibration sector is gone (tools/ble_calibration.py, from the unit's own full-flash backup).
 *   83 [0]                      the state
 *   83 [1, 115 packed bytes]    import a checked 100-byte copy (ed_unpack7 groups), taken at the radio's next start
 * The reply: result, version 1, the radio's calibration (0 not started, 1 stored, 2 measured), radio on, a link up, the
 * copy kept is in flash, its CRC (three 7-bit bytes, low first). Results: 0 ok, 1 not a valid copy, 2 a different copy is
 * kept, 4 busy (playing, or a note sounding: the save is a flash write), 5 the radio has not started.
 * The flash itself is never addressed by the caller (platform/ble_own.h saadet_ble_cal). */
#define ED_BLE_CAL 83u
#define ED_BLE_COPY 100u                                 /* (platform/ble/ble_vm.h BLE_RF_COPY_SIZE) */
static __attribute__((minsize)) int ed_ble_handle(uint32_t cmd, const uint8_t *a, uint32_t n)   /* (no hurry) */
{
    uint8_t copy[ED_BLE_COPY], st[4];
    uint32_t rc = 1, crc;
    int import;
    if (cmd != ED_BLE_CAL)
        return 0;
    import = n == 116u && a[0] == 1u && ed_unpack7(a + 1, n - 1u, copy, sizeof copy) == sizeof copy;
    if (import)
        rc = transport_busy() || sounding() ? 4u : 0u;   /* (as persist.c's saves: a flash write stops the audio) */
    else if (n == 1u && !a[0])
        rc = 0;
    if (!rc)
        rc = (uint32_t)sd_ble_cal(import ? copy : 0, st);
    else
        (void)sd_ble_cal(0, st);
    crc = (uint32_t)st[2] | (uint32_t)st[3] << 8;
    ed_b(rc);
    ed_b(1);
    ed_b(st[0]);
    ed_b((uint32_t)!!sd_ble_enabled());
    ed_b((uint32_t)!!sd_ble_linked());
    ed_b(st[1]);
    ed_b(crc);
    ed_b(crc >> 7);
    ed_b(crc >> 14);
    return 1;
}
