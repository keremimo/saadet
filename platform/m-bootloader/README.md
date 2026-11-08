# m-bootloader 2.3 (FM-1)

Unmodified release files of [m-bootloader](https://github.com/jvitkauskas/m-bootloader) by Julius Vitkauskas,
under the MIT licence in `LICENSE`. Release
[v2.3](https://github.com/jvitkauskas/m-bootloader/releases/tag/v2.3), commit
`6bc8f459b94f75cf8c0a94e4fb018b09b2cc7689`, build `2.3-6bc8f45`.

| File | Release asset | SHA-256 |
|---|---|---|
| `fm1-uboot-2.3.fwsc` | `fm1-uboot.fwsc` | `0299c41368f6d5d8f39776b3cad597a1c95677c27f3ca855c29368daf3d1d4e0` |
| `fm1-uboot-2.3.boot` | `fm1-uboot.boot` | `1a4ba056f98f9837720ee2353d8b541b08016a20389e32740d0c9e14d966f8a9` |

The checksums are the release's own `SHA256SUMS`. `tools/base_bootloader.py` refuses any other bytes, copies the
files into `platform/build/bootloader/` on every FM-1 base build, and the release site serves the `.fwsc` as the
installer's optional m-bootloader. The `.fwsc` holds only the bootloader's installer (identity `FM-1_099`): running
Saadet stages it through its update route, and it rewrites the boot area `[0, 0x4000)` and restarts into Saadet.
The `.boot` is the plain boot container, for M-Flasher and the base's boot-area test.

fm1-base's pin carries 1.2; Saadet serves 2.3 from here.
