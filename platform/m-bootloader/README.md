# m-bootloader 2.2 (FM-1)

Unmodified release files of [m-bootloader](https://github.com/jvitkauskas/m-bootloader) by Julius Vitkauskas,
under the MIT licence in `LICENSE`. Release
[v2.2](https://github.com/jvitkauskas/m-bootloader/releases/tag/v2.2), commit
`64f7c9cad1580bea870fc4a9e9bd61583fee3aef`, build `2.2-64f7c9c`.

| File | Release asset | SHA-256 |
|---|---|---|
| `fm1-uboot-2.2.fwsc` | `fm1-uboot.fwsc` | `cb6128ee753a1661e6399002317464959231854024f259520755621954438981` |
| `fm1-uboot-2.2.boot` | `fm1-uboot.boot` | `cedc9c17648bdeecda5c8d0ffffe2211849f8a36ab615fe49d6cc46d249baf44` |

The checksums are the release's own `SHA256SUMS`. `tools/base_bootloader.py` refuses any other bytes, copies the
files into `platform/build/bootloader/` on every FM-1 base build, and the release site serves the `.fwsc` as the
installer's optional m-bootloader. The `.fwsc` holds only the bootloader's installer (identity `FM-1_099`): running
Saadet stages it through its update route, and it rewrites the boot area `[0, 0x4000)` and restarts into Saadet.
The `.boot` is the plain boot container, for M-Flasher and the base's boot-area test.

fm1-base's pin carries 1.2; Saadet serves 2.2 from here.
