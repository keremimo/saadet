# FM-1 USB recovery

This command-line tool recovers an FM-1 **already in ROM UBOOT** over a normal USB
data cable. It does not force a running device into recovery. USB identity must be
`4c4a:8057`, with inquiry `WL82 / UBOOT1.00`. If you see a **Melodee Update** MIDI
port, resume installation with the web installer instead.

Download the archive for your computer from the Melodee 1.3.4 release and extract it.
Windows and Linux builds are x64; macOS has separate Intel/x64 and Apple Silicon/arm64
downloads. The executable includes Python, PyUSB, libusb and the RAM recovery helper.
It does not require a Python installation or the JieLi SDK. These are unsigned CLI
executables; use Terminal or PowerShell from the extracted directory.

Windows needs the **WinUSB driver for 4c4a:8057**. Use Zadig to install that driver
only for the ROM recovery device, not Melodee's normal MIDI/audio device. Linux may
need a udev rule or `sudo`. On macOS, USB access denial may require `sudo`.

Choose a `.fwsc` firmware package from the release downloads. The recovery tool
does not select or download firmware for you. To investigate a startup regression,
use a previously working version. Compare the file with the release's `SHA256SUMS`.

The corrected tool also supports the unmodified **official M-VAVE V15 `FM-1.fwsc`**
(identity `FM-1_015`). Download it from M-VAVE; it is not included in the recovery
archive. The complete file must have SHA-256
`db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`.
Modified V15 packages are refused before USB access. The recovery archives on the
1.3.1 release were corrected on 2026-10-13 to accept V15's metadata layout. If an
older download rejects V15, download the archive again.

For official recovery, substitute `FM-1.fwsc` in the commands below. Keep the full
backup: recovery retains the bootloader and saved-data bytes, but official firmware
may overwrite Melodee's songs, sounds or settings after it boots.

On macOS/Linux:

```sh
./fm1-recover --info
./fm1-recover /path/to/working.fwsc --check
./fm1-recover /path/to/working.fwsc
./fm1-recover /path/to/working.fwsc --write
```

On Windows PowerShell:

```powershell
.\fm1-recover.exe --info
.\fm1-recover.exe C:\path\to\working.fwsc --check
.\fm1-recover.exe C:\path\to\working.fwsc
.\fm1-recover.exe C:\path\to\working.fwsc --write
```

An FM-1 package recovers both 1 MiB flash parts the FM-1 ships with: the stock Puya
P25Q80 (JEDEC `856014`) and the UC25HQ80 (`B36014`) on units sold since autumn 2026.
Any other part is refused before flash is read. Melodee's recovery tool, which works
the same way, made a verified read-only backup of a UC25HQ80 unit
(keremimo/melodee#110). `--write` has not been tried on that chip.

`--info` identifies the ROM device without changing RAM or flash. `--check` validates
the package and bundled RAM helper without accessing USB. With a package and no
`--write`, the tool loads a temporary RAM helper and saves a full flash backup; it
does not change flash. `--write` makes a fresh backup, restores changed application
sectors, and verifies the whole flash. Keep the USB connection stable until it ends.

Backups go into unique directories under `fm1-recovery-backups/`; use `--backup-dir`
to choose another parent. Keep `full-flash-before.bin`, `backup.json` and
`retained-ram.bin`. The complete backup and manifest are synced to disk before the
first erase. The bootloader below `0x4000` and current saved data at/above `0x93000`
are retained. Legacy Prophet banks at `0x89000` are ignored and replaced by firmware;
they survive in the full backup, but are not relocated or loaded by current firmware.

Success prints **RESTORE VERIFIED** and creates `recovery-result.json`. Power-cycle
the FM-1 after recovery or a backup-only session. The report verifies flash contents;
check that the chosen firmware boots, plays audio, and responds to controls afterward.
If a write fails or USB disconnects, retain the original backup, reconnect in UBOOT
and retry. Completed sectors are skipped. Failed runs do not create a success report.

Connect one FM-1 at a time. `--bus` and `--address` select a device when necessary;
`--wait` waits up to 60 seconds for ROM USB. `--libusb PATH` can replace the bundled
library. `--loader PATH` can override the RAM helper with the same verified SDK file.
`--help` lists all options.

The direct-USB protocol has prior successful macOS recovery evidence. Each executable
is built and smoke-tested on its native OS; automated tests model interruptions and
protected flash regions. Those checks do not establish physical USB recovery on
every OS.

Source: https://github.com/keremimo/melodee (tag v1.3.4).
Protocol reference: https://github.com/kurogedelic/FM-1-transporter.
Licences and corresponding libusb source are included in the archive.

## Switching to another custom firmware

In the web installer, choose **Back up and install TransitFM1** before leaving Melodee
for another custom firmware. Once the FM-1 shows **TRANSIT**, open the destination
firmware's own installer and install it there. Use TransitFM1 as the stop between
firmwares instead of returning to official V15. It makes no sound and reports V15's
identity so other installers accept it.

The browser downloads TransitFM1 0.1.3 from its publisher and verifies its pinned
SHA-256 before sending anything to the FM-1. A complete Melodee backup is saved first;
**Skip backup** requires accepting possible data loss. Reinstall Melodee and restore
the archive in the editor to bring your data back.

**Baud Girl requires official V15 first**, followed by Baud Girl's own installer.
Its image can exceed the application area TransitFM1 writes (ending at `0x93000`).
TransitFM1 0.1.3 refuses oversized images before writing. Do not bypass an
installer's loader check: TransitFM1's running identity is `FM-1_015`, but its
loader answers `ota-FM-1_900` and does not write the whole flash like V15's loader.
Our Melodee installer accepts that loader for Melodee packages.

Do not use TransitFM1 0.1 (startup faults and installs stalled by incoming MIDI)
or 0.1.2 (withdrawn after a truncated Baud Girl install bricked a unit). Use 0.1.3,
and close DAWs or other apps sending MIDI during installation. See upstream's
[compatibility limits and version history](https://github.com/Quixotic7/TransitFM1/blob/main/docs/SAFETY.md).

### Transit is running, but the next installer cannot find it

The computer can retain the MIDI port name **Melodee** after TransitFM1 is
installed, even though the update handshake now correctly answers `FM-1_015`.
Sloop 2.7.1's browser and command-line installers filter MIDI port names before
sending that handshake; their default filters omit `Melodee`. The FM-1 Firmware
Hub's current filter also omits it. This can look like a firmware compatibility
failure before the installer has contacted Transit at all.

With Sloop's command-line installer and its verified release package, select the
retained port explicitly:

```sh
python3 tools/fm1_install.py sloop-2.7.1.fwsc --port Melodee --yes
```

Here `tools/fm1_install.py` is **Sloop's** installer. The browser installer needs
`melodee` added to its MIDI port-name filter (`web/fm1ota.js`, `FM1_PORT`); the
command-line default needs the same addition (`PORT_RE`). Keep the update-loader
checks intact. The port name and the firmware's handshake identity are separate.

TransitFM1 guards the next write against interruptions until the last sector; it
does not guarantee recovery from every power loss. If an install is interrupted,
resume **the same firmware**. For an interrupted TransitFM1 install, choose its button
again and select the saved-backup option (or explicitly skip backup). For an
interrupted destination install, use that firmware's installer. See the
[TransitFM1 instructions](https://quixotic7.github.io/TransitFM1/) for its update-mode
and recovery procedure.

## Return to stock from running Melodee

Use this option when you want to run official V15 itself. For switching between
custom firmwares, use TransitFM1 as described above.

Use **Return to official V15** in the web installer, or `tools/fm1_install.py FM-1.fwsc`,
with the unmodified M-VAVE `FM-1.fwsc`. The browser checks the file's SHA-256 locally;
the file is not uploaded. It is then sent unchanged, as M-VAVE's own updater sends it:
Melodee starts the official update loader from the file, and that loader installs the
whole V15 package. This includes the boot data below `0x4000`, the application, the
USR data at `0xEA000`, `isd_config.ini` and the package's other files.

Before Melodee starts the official loader, it compares V15's boot data with the FM-1's
own (`0x0`..`0x3FFF`), because that loader rewrites it. If they differ, the FM-1 shows
**FAILED -5** during **CHECK HEAD** and the installer stops. Nothing is written and
Melodee keeps running. The installer has no other way back to stock, and other tools
should not be used to force the return. Please
[report it](https://github.com/keremimo/melodee/issues) with the Melodee version,
the code on the FM-1's screen, and how Melodee or Felucca was first installed on it.

An interrupted return to stock is resumed only in the official update mode
(`ota-FM-1_015`): select the official file again and press **Return to official V15**.
If the FM-1 is in Melodee's update mode instead (**Melodee Update**, `ota-FM-1_900`),
press **Install** to finish Melodee first. The installer never sends the official file
to Melodee's loader, or a Melodee package to the official loader.

From 16 October 2026 until this correction, the web installer's
**Return to official V15** used Melodee's loader and wrote only V15's flash image
between `0x4000` and `0x92FFF`. It did not write V15's USR data or `isd_config.ini`,
and left Melodee's saved data in place. If you returned to stock in that period, please
report how your FM-1 behaves now.

The installer retains its full-backup and explicit skip-backup choices. Stock
firmware may change Melodee's saved data after boot. Restoring stock also removes
Melodee's application boot protection; keep power connected throughout the update.
