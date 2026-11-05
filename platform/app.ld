/* SPDX-License-Identifier: MPL-2.0
 * Modifications Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C08000 holds .ram_text (code that must run while the flash
 * is busy: flash drivers and radio calibration). Saadet audio code/tables
 * are copied into the POOL execution section before the worker is started.
 * RAM  : .data/.bss (96 KiB)
 * POOL : canvases, USB rings, radio tasks, resident audio and CPU1 stacks
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x8000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rwx)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    /* JieLi's BT code and constants run from XIP, as in their SDK (sdk_ld_sfc.c); .volatile_ram_code
     * and the radio trim (*.rf_trim, CONFIG_RF_TRIM_CODE_AT_RAM in their demos) run from RAM: the trim
     * draws ~500 ms of high current, and fetching from the flash meanwhile corrupts what is read
     * (the crashes 0.7 s into every BT start, fm1-ble notes/03-05) */
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*)
              *(.bt_stack_code .ble_sm_code .hci_controller_code .ble_hci_code .ble_att_code .ble_db_code
                .classic_lmp_code .link_bulk_code .classic_rf_code .lmp_irq_code .ble_gatt_code .ble_ll_code
                .ble_rf_code .bt_rf_code .hci_interface_code .vendor_manager_code .device_manager_code)
              *(.*_const)
              . = ALIGN(4); _etext = .; } > XIP
    /* .c1_text: an app's second core's RAM loop (tools/build.py), copied and guarded with the rest */
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) *(.volatile_ram_code) *(*.rf.text.cache.L2.rf_trim)
                  . = ALIGN(4); _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1_end = .;
                  _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) *(.*_data) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) *(.*_bss) *(.bd_base) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); } > POOL
    .audio_text : { _sd_audio_start = .; *(.audio_text*) *(.audio_libc*) *(.dsp_text*) *(.dsp_tables*) . = ALIGN(4); _sd_audio_end = .; } > POOL AT > XIP
    _sd_audio_load = LOADADDR(.audio_text);
    _sd_exec_end = _sd_audio_end;
    .cpu1_stacks (NOLOAD) : { . = ALIGN(32); _cpu1_stacks_lo = .; KEEP(*(.cpu1_stacks)) _cpu1_stacks_hi = .; } > POOL
    _cpu1_ustack_top = _cpu1_stacks_lo + 4096;
    _cpu1_sstack_top = _cpu1_stacks_lo + 8192;
    .audio_scratch (NOLOAD) : { *(.audio_scratch*) } > POOL
    _pool_end = ALIGN(., 32);
    _resource_start = _pool_end;
    _resource_end = 0x01C74000;
    ASSERT(_resource_end - _resource_start >= 32768, "Saadet engine bank below 16 KiB a part")
    _resource_low_start = 0; _resource_low_end = 0;
    _resource_data_start = 0; _resource_data_end = 0;
    _resource_retained_start = 0; _resource_retained_end = 0;
    _resource_cache_start = 0; _resource_cache_end = 0;
    .noinit (NOLOAD) : { *(.noinit*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
