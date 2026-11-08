#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""tools/ble_calibration.py: the copy taken from a backup, its checks, and editor command 83 against a fake unit.
tests/base_ble_own_test.py checks the same copy against the firmware's own (platform/ble/ble_vm.c)."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import ble_calibration as cal


def record(ident, data):
    n = len(data)
    return bytes([cal.crc16(data) & 255, ident & 255, (ident >> 8) | (n & 15) << 4, n >> 4]) + data


def image_at(offset=0xE8000):
    im = bytearray(b'\xff' * 0x100000)
    records = {i: bytes(range(n)) for i, n in cal.LENGTHS.items()}
    records[187] = records[187][:64] + cal.crc16(records[187][:64]).to_bytes(2, 'little') + b'\0\0'
    vm = b'\x55\xaa\xaa\x55' + b''.join(record(i, d) for i, d in records.items())
    im[offset:offset+len(vm)] = vm
    return im, records


class Unit:
    """Command 83 as platform/ble_own.h answers it: a kept copy, saved by a later poll."""
    def __init__(self, radio=2, kept=None, saves=True):
        self.radio, self.kept, self.saves, self.flash, self.calls = radio, kept, saves, None, []

    def reply(self, rc):
        crc = int.from_bytes(self.kept[98:], 'little') if self.kept else 0
        saved = int(self.kept is not None and self.flash == self.kept)
        if self.kept and self.saves:
            self.flash = self.kept                     # (the next poll's save)
        return [rc, 1, self.radio, 1, 0, saved, crc & 127, (crc >> 7) & 127, crc >> 14]

    def request(self, cmd, args):
        assert cmd == 83
        self.calls.append(list(args))
        if list(args) == [0]:
            return self.reply(0)
        assert len(args) == 116 and args[0] == 1 and all(x < 128 for x in args)
        wire = args[1:]
        copy = bytes(x | ((wire[i] >> j & 1) << 7) for i in range(0, len(wire), 8) for j, x in enumerate(wire[i+1:i+8]))
        if not self.radio:
            return self.reply(5)
        if self.kept and self.kept != copy:
            return self.reply(2)
        self.kept = copy
        return self.reply(0)


class Calibration(unittest.TestCase):
    def test_extract_and_checks(self):
        image, rec = image_at()
        area, copy = cal.extract(image)
        self.assertEqual(area, 0xE8000)
        self.assertEqual(copy[0], 0xAC)
        self.assertEqual(copy[1:98], b''.join(rec.values()))
        self.assertTrue(cal.copy_valid(copy))
        self.assertFalse(cal.copy_valid(copy[:-1]))
        bad = bytearray(copy); bad[94] ^= 1
        bad[98:] = cal.crc16(bad[:98]).to_bytes(2, 'little')
        self.assertFalse(cal.copy_valid(bad))

    def test_invalid_backups(self):
        for image in [bytes(1024), bytes(0x100000), b'\xff' * 0x100000]:
            with self.assertRaises(ValueError): cal.extract(image)
        image, _ = image_at(); image[0xE8008] ^= 1
        with self.assertRaises(ValueError): cal.extract(image)

    def test_partial_and_conflicting_sets(self):
        image, rec = image_at()
        start = 0xE8004 + sum(4+len(rec[i]) for i in [106, 107, 108])
        image[start] ^= 1
        with self.assertRaises(ValueError): cal.extract(image)
        image, rec = image_at()
        rec[106] = b'\x40\x41'
        vm = b'\x55\xaa\xaa\x55' + b''.join(record(i, d) for i, d in rec.items())
        image[0xE7000:0xE7000+len(vm)] = vm
        with self.assertRaisesRegex(ValueError, 'conflicting'): cal.extract(image)

    def test_pack_is_the_editors(self):
        _, copy = cal.extract(image_at()[0])
        wire = cal.pack(copy)
        self.assertEqual(len(wire), 115)
        self.assertEqual(bytes(x | ((wire[i] >> j & 1) << 7) for i in range(0, len(wire), 8)
                               for j, x in enumerate(wire[i+1:i+8])), copy)

    def test_restore(self):
        _, copy = cal.extract(image_at()[0])
        unit = Unit()
        before, after = cal.restore(unit, copy, wait=1)
        self.assertEqual(before['radio'], 'measured')
        self.assertFalse(before['saved'])
        self.assertTrue(after['saved'])
        self.assertEqual(after['crc'], int.from_bytes(copy[98:], 'little'))
        self.assertEqual(unit.kept, copy)
        # the same copy again: nothing to do, still saved
        _, again = cal.restore(unit, copy, wait=1)
        self.assertTrue(again['saved'])

    def test_refusals(self):
        _, copy = cal.extract(image_at()[0])
        other = bytearray(copy); other[1] ^= 1
        other[98:] = cal.crc16(other[:98]).to_bytes(2, 'little')
        with self.assertRaisesRegex(RuntimeError, 'different calibration'):
            cal.restore(Unit(radio=1, kept=bytes(other)), copy, wait=1)
        with self.assertRaisesRegex(RuntimeError, 'has not started'):
            cal.restore(Unit(radio=0), copy, wait=1)
        with self.assertRaisesRegex(RuntimeError, 'not confirmed saved'):
            cal.restore(Unit(saves=False), copy, wait=.3)
        with self.assertRaises(ValueError):
            cal.restore(Unit(), copy[:-1], wait=1)

    def test_old_firmware(self):
        class Battery:                                  # (81 is the battery's: six bytes, another schema)
            def request(self, cmd, args): return [1, 0, 0, 0, 0, 0]
        with self.assertRaisesRegex(RuntimeError, 'update Saadet'):
            cal.status(Battery())


if __name__ == '__main__':
    unittest.main()
