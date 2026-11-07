# Saadet brand

Saadet is synthesizer firmware for the M-VAVE FM-1 and, in beta, the M-VAVE SMK-37, made by Ellic Studio, with Saadet
Studio, its editor in the browser. The brand is a dark instrument panel at night: an ink ground, cream type, and one
colour per engine. It should look as calm as the music people make on it and as exact as the release notes read.

The files here are made by [`tools/gen_brand.py`](../../tools/gen_brand.py); change the script and run it again rather
than editing the SVGs. The colour names are Saadet Studio's tokens (`packages/saadet-ui/src/tokens.css` there).

| File | For |
| --- | --- |
| `saadet-wordmark.svg`, `saadet-wordmark-ink.svg` | The wordmark: cream on dark grounds, ink on light ones |
| `saadet-mark.svg`, `saadet-mark-ink.svg` | The sine S alone, for avatars and where the name is already present |
| `saadet-icon.svg`, `saadet-icon-512.png`, `-180.png`, `-32.png` | The app icon, apple-touch icon and favicon |
| `saadet-strands.svg` | The six strands, one per engine, for dark grounds |
| `social-preview.png` | GitHub's social preview, 1280 x 640 |
| `introducing-saadet.svg` | The source of [`../readme/introducing-saadet.png`](../readme/introducing-saadet.png), the README's introduction and the release picture |

## Palette

| Token | Dark | Light | Use |
| --- | --- | --- | --- |
| `bg` | `#12161d` | `#f6f3ea` | The ground: pages, graphics, the icon's tile. |
| `panel` | `#151a22` | `#efebe0` | Side bars and the app shell around the work area. |
| `surface` | `#1a2029` | `#ffffff` | Cards and editor sections that sit on bg. |
| `raised` | `#222a35` | `#f1ede3` | Controls at rest on surface: buttons, segments, inputs. |
| `raised-2` | `#2a3340` | `#e6e1d4` | Controls hovered or pressed; a second step above raised. |
| `line` | `#2d3644` | `#ddd7c8` | Hairlines between sections and a waveform's zero line. |
| `line-2` | `#3a4555` | `#cbc4b3` | Stronger rules: the footer rule on graphics, control outlines at rest. |
| `text` | `#efe9d8` | `#1b1f26` | The wordmark, headings and body copy on bg, surface and raised (11.9:1 or more in both themes). |
| `secondary` | `#c9cfd9` | `#39414d` | Secondary copy on bg, surface and raised (8.8:1 or more). |
| `mid` | `#a3acba` | `#59626f` | Sub-lines under headlines, captions, stat labels (5.3:1 or more on bg, surface and raised). |
| `quiet` | `#8590a0` | `#66707d` | Uppercase labels and the version beside the wordmark, on bg and surface only (5.0:1 or more; 4.3:1 on raised, so not there). |
| `dim` | `#6f7a8a` | `#8a929d` | Disabled controls and placeholders only; never for text someone must read (under 4.5:1). |
| `eng-prophet` | `#f2876b` | `#c2512f` | The Prophet engine only: its name, its waveform, its chips and knob strip. |
| `eng-fm6` | `#86bfe8` | `#2f7fb8` | The FM6 engine only. |
| `eng-cz1` | `#e8cd78` | `#9a7a12` | The CZ-1 engine only. |
| `eng-juno106` | `#b2dc86` | `#4f8a22` | The JUNO-106 engine only. |
| `eng-hydra` | `#c5a5f0` | `#7b55c4` | The HYDRA engine only. |
| `eng-ppg` | `#6fd3ea` | `#1a86a3` | The PPG engine only (the unit's cyan on the panel). |
| `eng-gr1` | `#79d2c4` | `#1f8a7a` | Reserved for the granular engine, which is parked. |
| `ok` | `#7fd99a` | `#23824a` | Connected, saved, passed. |
| `warn` | `#f5dfa0` | `#8a6a10` | Needs attention: a backup not taken, a beta build. |
| `danger-text` | `#f2a08a` | `#b4442a` | Errors and destructive actions (Delete, Erase). |

## Type styles

| Style | Face | Use |
| --- | --- | --- |
| `hero` | Sora 300, 150px/1, 0.06em | The wordmark's size on the README introduction. |
| `headline` | Sora 400, 46px/1.15 | One line under the wordmark, in text. |
| `stat` | Sora 400, 46px/1.1 | A real count on a graphic, with its stat-label under it. |
| `title` | Sora 400, 28px/1.2 | Screen and section titles in Saadet Studio and on pages. |
| `lede` | Manrope 400, 29px/1.3 | The sub-line under a headline, in mid. |
| `stat-label` | Manrope 400, 25px/1.3 | Under a stat, lower case, in mid. |
| `body` | Manrope 400, 14px/1.45 | Saadet Studio's base size and all running text. |
| `body-strong` | Manrope 600, 14px/1.45 | The one sentence in a paragraph that someone must not miss. |
| `eyebrow` | Manrope 600, 20px/1.2, 0.16em | The endorsement line and the hardware line at the top of a graphic, uppercase, in quiet. |
| `engine` | Manrope 700, 20px/1.2, 0.12em | An engine's name in its own eng- colour, uppercase, as the firmware prints it. |
| `label` | Manrope 600, 13px/1.3, 0.12em | Uppercase captions and control labels; panel buttons as printed (HOME, ARP, SELECT). |
| `value` | JetBrains Mono 400, 13px/1.4 | Values, SysEx bytes, hashes, release identities (FM-1_9010000) and file names. |

## Names

- Write **Saadet** in running text. Use **SAADET** only as the wordmark and inside uppercase labels.
- The editor is **Saadet Studio**. The SMK-37 builds are **Saadet Pro**, **Saadet Elite** and **Saadet MKE**.
- The maker line is `ELLIC STUDIO / SAADET`, in `eyebrow`, top left of a graphic.
- Name the hardware in full the first time: **M-VAVE FM-1**, **M-VAVE SMK-37**, **SMK-37 Elite**. After that, FM-1 and SMK-37.
- Write the engines as the firmware prints them: **Prophet**, **FM6**, **CZ-1**, **JUNO-106**, **HYDRA**, **PPG** in prose;
  uppercase (PROPHET) in `engine` labels. They always appear in that order.
- Versions: **Saadet 1.0** in prose, `1.0` beside the wordmark. The release identity is mono: `FM-1_9010000`.
- Other people's trademarks (M-VAVE, FM-1, SMK-37, PPG, Wave 2.2, Hydrasynth, Prophet-5, JUNO-106, CZ-1, DX7) belong
  to their owners; say so in the footer of anything public. Saadet is an independent project.

## Voice

Write the way the release notes are written: plain, exact and honest about what has and has not been checked.

- State what happens, with the number and its unit: "The old voice fades over 23 ms under the new note, which starts at
  once." "The limiter now takes only what goes over -2 dBFS, up from -5 dBFS."
- Say what is measured and what is not: "Its sound has been measured, not yet judged by ear."
- Panel controls are uppercase as printed: HOME, ARP, SELECT, MASTER. Pages too: GLOBAL > PARTS.
- Use second person for instructions ("Back up any sounds you want to keep"), first person plural for the project
  ("our own waves").
- Sentence case for headings and buttons. Headlines are short sentences with full stops:
  "Six engines. Two parts. A new editor."
- No superlatives, no hype words, no exclamation marks, no emoji.

## Logo

The mark is one cycle of a sine wave stood on its end, so it reads as an S. In the wordmark it is the first letter
of SAADET; the other letters are Sora 300, uppercase, 6 % of the size apart.

- Use `saadet-wordmark.svg` (cream, `text` dark, #efe9d8) on `bg` dark and `saadet-wordmark-ink.svg` (ink, `text`
  light, #1b1f26) on `bg` light. Never another colour, never an outline, never per-letter colour.
- Use the mark alone (`saadet-mark.svg`, `saadet-mark-ink.svg`) only where the name is already present or there is
  no room: an avatar, a favicon, the app icon (`saadet-icon.svg`, the mark on `bg` dark at `radius-icon`).
- Keep one cap height clear on every side of the wordmark. Do not set it narrower than 96 px; below that, use the mark.
- Never draw a vertical line through the S: with its zero line it reads as a dollar sign.
- Never retype the wordmark in a font: the S is drawn, not a glyph. Rotate, stretch or animate it only as the whole S.
- The version sits on the wordmark's baseline at the same size, Sora 300, in `quiet`.

## Colour

- Lead with `bg` dark and `text`. Brand graphics are always dark; Saadet Studio follows the viewer's theme.
- Lift surfaces in order: `bg`, `panel`, `surface`, `raised`, `raised-2`. Separate them with `line` hairlines, not
  shadows.
- Write body copy in `text` or `secondary`, sub-lines and captions in `mid`, uppercase labels in `quiet`. Keep `dim`
  for disabled controls.
- An `eng-` colour means that engine and nothing else: its name, its waveform, its chips and knob strip. Never use
  one as a general accent, a link colour or decoration.
- The six together, in engine order, appear only in the six strands and in the row of engine names.
- On light, the engine colours reach 3.5 to 4.7:1: use them for marks and labels of 18 px or more, and set small text
  in `text` with an engine-colour mark beside it.
- `ok`, `warn` and `danger-text` always come with a word. They never decorate.
- The focus ring is 2 px solid `text`, offset 2 px.

## Type

- `display` (Sora) for the wordmark, headlines, titles and big numbers: `hero`, `headline`, `stat`, `title`.
- `sans` (Manrope) for everything read: `lede`, `body`, `body-strong`, and the uppercase `eyebrow`, `engine` and `label`.
- `mono` (JetBrains Mono) for values a person might copy: SysEx, hashes, release identities, file names: `value`.
- Uppercase only for labels, and always letter-spaced (0.12 to 0.16 em). Never set a sentence in uppercase.
- Use tabular figures wherever numbers line up.
- The firmware's own screen keeps its own pixel font; this type is for everything around it.

## Graphics

- **Waveforms.** Each engine is drawn as two cycles on a `line` zero line, in its `eng-` colour, 3.5 px round-jointed:
  a saw for Prophet, a three-times modulator for FM6, a phase-bent cosine for CZ-1, a 35 % pulse for JUNO-106, a sine
  turning square for HYDRA and a stepped table for PPG. They are drawings of each engine's character, not measured
  audio; say so wherever they are used.
- **The six strands.** Six S curves side by side in the engine colours, in engine order: the 1.0 graphic for the
  social preview and release posts. Let them bleed off an edge.
- **Screens.** Show the firmware's screen as rendered by `tests/ui_harness.c`: unretouched 240 x 240, enlarged by whole
  numbers with nearest-neighbour sampling. Never mock a screen.
- **Hardware.** Draw the FM-1 and SMK-37 flat and as they are; never photograph-style or invented controls.
- **Layout.** Left-aligned, with the eyebrow top left, the hardware top right, a `line-2` rule above the footer and
  the URLs in the footer (github.com/keremimo/saadet left, saadet.ellicstudio.com right).
- Every word on a graphic is outlined (`tools/gen_brand.py` does this), so it renders without the fonts installed.

## Iconography

Saadet has no icon set. Saadet Studio uses words and the panel's own names for controls, and draws engine waveforms,
envelopes and filter curves from the sound being edited. Use no emoji and no stock icons; where a symbol is
unavoidable, draw it in `text` at the stroke weight of `body`.
