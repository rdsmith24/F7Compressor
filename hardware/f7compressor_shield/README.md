# F7Compressor carrier PCB (NUCLEO shield)

KiCad 10 project for a board that the NUCLEO-H723ZG plugs into directly,
replacing the point-to-point jumper wiring in `../../wiring_diagram.html`
between the NUCLEO and the LCD module, relay module, oil-pressure switch,
e-stop, and 4-20mA pressure loop. Open `F7CompressorShield.kicad_pro`.

## What's actually in this first pass

- **Schematic (`.kicad_sch`)**: complete and ERC-clean (0 errors, 0 warnings).
  Every net is named after the signal names used elsewhere in this repo
  (`FMC_D0`, `TP_SCK`, `RELAY_RUN`, `OIL_SW`, `ESTOP`, `PRESSURE_SENSE`,
  etc.) and was transcribed pin-by-pin from the FMC/SPI5/Analog&Digital I/O
  tables in `wiring_diagram.html`. Pins that are intentionally not used are
  marked with no-connect flags rather than left silently floating, so the
  schematic documents "unused" vs. "forgot to wire" explicitly.
- **PCB (`.kicad_pcb`)**: all 23 footprints (14 components + 4 LCD mounting
  holes + 5 Nucleo mounting holes) placed with correct net assignments, on
  **one unified board outline** (191.23 x 182.65mm). **Fully routed**: all
  39 nets, 2 layers, 491 track segments, 27 vias, 0 unconnected items —
  see "Routing" below for how.
- Two footprint groups on that one outline:
  - **NUCLEO-mating cluster** (`U1`, `CN7`-`CN10`), top-left: laid out to
    mirror where these connectors actually sit under the real NUCLEO-144
    board, with a **true-to-size reference outline** (70 x 133.34mm —
    the full board *with* the ST-Link section, since that's staying
    attached; `Dwgs.User` layer) drawn around it and 5 mounting holes
    (4 corners + 1 off-center, matching UM1974 Figure 7) — see "Mechanical
    layout" and "Nucleo mounting holes" below.
  - **Field-wiring group** (`J1`, `J2`, `SW1`-`SW3`, `J5`, `J6`, `R1`, `C1`, plus
    4 unreferenced LCD mounting-hole footprints): the LCD header (`J1`)
    top-right — positioned and surrounded by 4 mounting holes that
    reproduce the real LCD module's hole pattern, see "LCD mounting" below
    — with the relay header, simulation toggle switches, and pressure-loop
    terminal stacked below it, i.e. to the right of the Nucleo cluster and
    down from the LCD, per request. Each part is left-aligned and stacked
    using its own real footprint bounding box (`stack_right()` in
    `gen_pcb.py`), so the gaps stay correct no matter how large or
    asymmetric an individual footprint's silkscreen/pads are (the terminal
    block and the resistor, in particular, both extend well to one side of
    their placement origin).
- Reference designators: `CN7`–`CN10` are the NUCLEO Zio headers (female
  sockets, mate directly with the NUCLEO board), `U1` is the Morpho
  connector pair (CN11+CN12, see below), `J1` is the LCD module header,
  `J2` is the relay module's control header, `SW1`-`SW3` are the on-board
  MTS-1 toggle switches (oil switch / e-stop / motor run sense), `J5` is
  the pressure-loop field terminal, `J6` is the 5VDC power input terminal
  (see "Power input" below), `R1`/`C1` are the pressure loop's 150Ω shunt
  and 100nF decoupling cap.

## Oil switch / e-stop / run-sense: on-board MTS-1 switches, not field terminals

`OIL_SW` and `ESTOP` used to be 2-pin screw terminals for wiring in the
real field switches. They're now **on-board MTS-1 SPDT toggle switches**
(`SW1`, `SW2`) instead, so the oil-pressure-fault and e-stop conditions can
be flipped directly on the bench without needing the actual field hardware
wired up — useful for firmware bring-up. `SW3` adds a third switch for a
**`RUN_SENSE`** net simulating motor-running feedback (see below).

- Each switch is used as a simple SPST: the common pole (pin 2, physically
  the MTS-1's center pin) goes to the signal net (`OIL_SW` / `ESTOP` /
  `RUN_SENSE`), one throw (pin 1) goes to `GND`, and the third throw (pin 3)
  is left unconnected. Toggling the switch mimics a NC switch's two states —
  closed (pin2–pin1 connected, net pulled to GND) or open (net floating,
  pulled HIGH by the MCU's internal pull-up).
- No bundled KiCad footprint matches the MTS-1's real pin spacing, so
  `gen_pcb.py`'s `make_mts1_footprint()` builds it directly: 3 round THT
  pads at the standard 4.7mm MTS-1/MTS-2/MTS-3 "PC pin" pitch, with the
  center pad (pin 2, common) drawn square to mark pin 1. Double-check this
  pitch against the actual MTS-1 you have on hand before populating.
- `J3`/`J4` (the old terminal references) no longer exist; `J5` (pressure
  loop) is unchanged since it isn't a switch — the 4-20mA loop still needs
  a real field terminal, not a toggle.

### `RUN_SENSE` (`SW3`) — new signal, not in firmware yet

There is **no existing motor-running-feedback input anywhere in this
project** — `CLAUDE.md`'s state machine only ever commands the relay
(`RELAY_RUN`), it never reads back whether the motor actually started. This
switch wires up a net for that *future* feature, the same way the spare
relay channels (`RELAY_IN2`-`4`) were wired for future use:

| Net | MCU pin | NUCLEO connector | Configured anywhere today? |
|---|---|---|---|
| `RUN_SENSE` | `PC3` | CN9 pin 5 (Zio) | No — not in CubeMX, not in `CLAUDE.md`, not read by any firmware |

`PC3` was picked because, like `PD6`/`PB1` for the spare relay channels,
it's unclaimed by any peripheral in `CLAUDE.md`'s pin tables. Using it for
real motor-run sensing later would need: CubeMX GPIO-input config for
`PC3`, a firmware read of it (e.g. in `compressor_sm.c`'s `RUNNING` state
to detect a stall/failed-start condition), and deciding what the state
machine should actually *do* with that information — none of which this
PCB-only change touches.

## Relay header (J2) — all 4 channels wired

The relay module is a 4-channel board but the firmware only drives channel
1 (`RELAY_RUN` = `PG4`, CN12 pin 69). The other 3 channels are now wired on
this board too, for future use, even though nothing drives them yet:

| J2 pin | Net | MCU pin | NUCLEO connector | Driven by firmware today? |
|---|---|---|---|---|
| 3 (IN1) | `RELAY_RUN` | `PG4` | CN12 pin 69 (Morpho) | Yes — `relay.c` |
| 4 (IN2) | `RELAY_IN2` | `PG5` | CN12 pin 68 (Morpho) | No — CubeMX already configures it as a GPIO output (`CLAUDE.md`), just unused |
| 5 (IN3) | `RELAY_IN3` | `PD6` | CN9 pin 4 (Zio) | No — not configured by CubeMX at all yet |
| 6 (IN4) | `RELAY_IN4` | `PB1` | CN9 pin 7 (Zio) | No — not configured by CubeMX at all yet |

`PG5`/`PD6`/`PB1` were picked because they're genuinely unclaimed by any
other peripheral in `CLAUDE.md`'s pin tables. Before IN2–IN4 can actually
be used, `PD6` and `PB1` need to be configured as GPIO outputs in CubeMX
(`PG5` already is) and the firmware needs a `Relay_Request()`-style call
added for each channel — this board only adds the physical wiring, it
doesn't change firmware.

## Power input (J6)

`J6` is a 2-pin screw terminal (same `TerminalBlock` family as `J5`,
5.08mm pitch) for an external 5VDC supply, so one connector on this board
can power the Nucleo and everything downstream of it.

- `J6` pin 1 (`+5V_IN`) goes to `U1` pin 6 — **Morpho CN11 pin 6, E5V** —
  the documented primary power source for this whole project
  (`wiring_diagram.html`'s Power table: "External 5 VDC adapter | CN11
  pin 6 (E5V) | Primary power source"). `J6` pin 2 goes to `GND`.
- **This is deliberately a separate net from the existing `+5V`** used for
  the relay module's VCC (sourced from Zio `CN8` pin 9). `CN8` pin 9 is
  the *Nucleo's own* regulated pass-through output, derived from E5V
  through the Nucleo's on-board power-source mux/protection circuitry —
  tying `+5V_IN` directly to it on our board would bypass that circuitry
  instead of feeding it properly. Once E5V is powered, `CN8` pin 9 should
  carry the same rail anyway (via the Nucleo's own hardware), so the
  relay's `+5V` wiring doesn't need to change.
- **A jumper move on the physical Nucleo board is required for this to
  actually work**: `JP2` has to be set to the `EXT` position (pins 5-6),
  per `CLAUDE.md`'s hardware summary — this is a jumper on the Nucleo
  itself, not something this shield can set for you.
- A `PWR_FLAG` symbol is attached to `+5V_IN` in the schematic — not a
  real component, just a standard KiCad marker asserting "this net is
  externally driven," needed because `E5V` is a `Power input`-typed pin
  that ERC otherwise expects a driving `Power output` pin for.

## What's routed vs. just present

Only the nets actually used by this design are wired — everything else on
the Zio/Morpho headers is populated (for mechanical fit) but left on
no-connect. See `CLAUDE.md` for the full signal-to-pin reference this was
built from.

## Morpho: CN11 and CN12 are real, separate connectors (not a combined part)

Three FMC data lines (`PD8`/`PD9`/`PD10` = `FMC_D13`-`D15`) and the RUN
relay (`PG4`) are only reachable on the Morpho connectors (CN11/CN12),
which are unpopulated from the factory — you solder male pin headers onto
the Nucleo at those two locations, and this shield needs matching female
sockets.

**This went through two iterations.** The first used KiCad's bundled
`Module:ST_Morpho_Connector_144_STLink` footprint (via the official
`MCU_Module:NUCLEO144-F429ZI` symbol) as a single combined part for both
CN11 and CN12, with pad numbers cross-checked against `CLAUDE.md`'s known
signals (pad N = CN11 pin N for N 1-72, pad N = CN12 pin N-72 for N
73-144 — `PD9`→pad 69→CN11 pin 69, `PG4`→pad 141→CN12 pin 69, `PD8`→pad
82→CN12 pin 10, `PD10`→pad 137→CN12 pin 65, all four verified). That
worked electrically, but direct inspection (prompted by counting pins on
the real board) found two real problems with it:

- The real `CN11`/`CN12` are each **35×2 (70-pin)** headers, confirmed
  from the NUCLEO-H723ZG schematic (sheet 5, "MORPHO CONNECTOR LEFT/RIGHT
  SIDE", labeled "Header 35x2"). `CLAUDE.md` previously said 2×38, which
  was wrong and has been corrected. The bundled footprint has 74 pads per
  side (148 total) — 8 more than the real connectors.
- It's **one footprint representing two physically separate connectors**,
  which doesn't match the real board and isn't something you can order as
  a single part.

**Current design**: `CN11` and `CN12` are two separate instances of the
same plain `Connector_Generic:Conn_02x35_Odd_Even` / `PinSocket_2x35_
P2.54mm_Vertical` part already used (and pin-1-verified) for `CN7`-`CN10`
— exactly 70 pads each, nothing extra. Pin numbers and GPIO assignments
are ST's own real-board numbering, from UM1974 Table 21 ("ST morpho
connector for NUCLEO-F207ZG, ..., NUCLEO-H743ZI" — the same family this
H723ZG board belongs to) — cross-checked against `CLAUDE.md` and found to
match exactly for all six signals actually used (`FMC_D13`→CN12 pin 10,
`FMC_D14`→CN11 pin 69, `FMC_D15`→CN12 pin 65, `RELAY_RUN`→CN12 pin 69,
`RELAY_IN2`→CN12 pin 68, `+5V_IN`→CN11 pin 6).

**Pin-1 orientation for CN11/CN12**, determined the same rigorous way as
`CN7`-`CN10`/`J1` (see below) rather than assumed: zoomed into UM1974's
whole-board mechanical drawing (Figure 6) at both connectors' own pads and
found the square pin-1 marker on the **left column** for both — same
absolute direction for both connectors, not mirrored between them, even
though CN11 and CN12 sit on opposite sides of the board. Placed using the
same `place_mirrored()` helper as the Zio headers.

This closes Open Item 1 from the original plan — the Morpho pad mapping
is no longer a guess, and the connectors now mechanically match the real
board part-for-part.

## Mechanical layout (grounded in ST's own drawing, not arbitrary anymore)

The first draft of this board spread `CN7`-`CN10` and `U1` arbitrarily far
apart purely to dodge DRC overlap — that didn't look or behave like a real
shield, and isn't what a shield should look like. This pass fixes that
using ST's own **UM1974 "Nucleo-144 board mechanical drawing in
millimeter"** (Figure 6), fetched from st.com, plus the bundled
`ST_Morpho_Connector_144_STLink` footprint's own verified pad geometry:

- The footprint's own CN11 pads sit at local x = 0-2.54mm and CN12 pads at
  x = 60.96-63.5mm — a 63.5mm outer span that matches UM1974's dimensioned
  "63.50mm" cuttable-target-board width exactly, confirming the footprint's
  internal coordinate frame is physically accurate and can be used directly
  as the board-cluster frame, with `U1`'s own placement origin landing
  exactly on CN11 pin 1.
- `CN7`/`CN8`'s pin-1 offsets *from that same origin* were pixel-measured
  directly off the drawing (centroid-detecting each header's square pin-1
  pad in a 600dpi render of UM1974 Figure 6, calibrated against the
  header's own known 2.54mm pin pitch measured the same way):
  `CN8` ≈ (7.48, 9.78) mm from CN11 pin 1, `CN7` ≈ (53.66, 0.22) mm from
  CN12's first pad. `CN9`/`CN10` are stacked directly below `CN8`/`CN7`
  using each header's own real pin-row span (17.78mm / 22.86mm) plus a
  2.54mm gap — they physically stack in one column on the real board too.
- This replaced an earlier version that *guessed* the Zio inboard offset
  (~5-10mm, not measured) — that guess turned out to collide with `U1`'s
  real pads once real mounting holes were added (see "Nucleo mounting
  holes" below), which is what prompted re-measuring `CN7`/`CN8` properly
  instead of patching around the collision.

**What's still approximate**: this is a hand pixel-measurement of a scanned
legacy drawing (UM1974 shows the original MB1137 board, not this project's
exact MB1364/H723ZG revision — ST keeps Zio/Morpho positions identical
across the whole Nucleo-144 family specifically so shields stay compatible
across revisions, which is why this is still a reasonable basis), with
roughly 1-2mm / 2-3% of calibration uncertainty based on cross-checking
the measured `CN12` pad position against its known-exact footprint
location. Good enough to avoid real collisions (confirmed below), not a
substitute for checking against UM2407 (the H7-specific manual) or the
physical board before fab.

**Expected DRC findings from this intentional overlap**: because the
NUCLEO-mating cluster deliberately places `CN7`-`CN10` inside `U1`'s
courtyard (that's the whole point — the Zio pins really do sit in the same
board area the Morpho pins occupy on a real stacked shield), DRC reports
courtyard-overlap / PTH-inside-courtyard violations between `U1` and each
of `CN7`-`CN10`, plus a couple of silkscreen-clearance warnings from the
tight stacking. These are expected for this design pattern, not placement
mistakes — any real NUCLEO-144 shield hits the same checks. They'd
normally be added as DRC rule exclusions once the layout is final rather
than "fixed" by spacing things out again.

**True-size reference outline**: an earlier pass drew only a tight
bounding box around `U1`/`CN7`-`CN10` as the "Nucleo footprint," which
looked (and was) noticeably smaller than a real Nucleo-144 board. There's
now a proper **70 x 133.34mm reference rectangle** (`Dwgs.User` layer, not
`Edge.Cuts` — same treatment as the LCD outline below) at the board's real
size. It's anchored so its top-left corner sits at the standard board
margin; the 6.5mm of width beyond the Morpho footprint's own 63.5mm span
is a confirmed number from UM1974's dimension chain, split evenly
left/right as a documented assumption (the drawing doesn't say how it
splits); the 44.44mm of height beyond the Morpho pad span is centered
top/bottom the same way. Treat this outline as a size/scale reference, not
a precise registration aid.

## Nucleo mounting holes

5 `MountingHole_3.2mm_M3` footprints (no net), for standoffs supporting
the Nucleo board (ST-Link section included — see above).

UM1974 Figure 6 (mm) / Figure 7 (mil, same drawing) shows **5** filled-dot
markers — 2 at the ST-Link/target cut line (near `CN8`/`CN7`), 2 further
down (near `CN9`/`CN10`), and a 5th ~2/3 of the way down the board, offset
right of center (near the ADC_IN/QSPI boundary on the right-hand header),
not at the middle of the other 4. Each dot's pixel position was
centroid-detected in the same 600dpi render used for `CN7`/`CN8` above,
and converted to mm **relative to CN11 pin 1** (`U1`'s own placement
origin) using the same local pin-pitch calibration — i.e. the holes and
the headers are measured in one consistent frame now, not two separate
ones:

| Hole | Position (mm from `U1`'s origin) | Near |
|---|---|---|
| top-left | (7.91, -3.39) | `CN8`/`CN11` top |
| top-right | (56.96, -3.00) | `CN7`/`CN12` top |
| middle | (41.48, 49.26) | right header, ADC_IN/QSPI boundary |
| bottom-left | (9.39, 73.70) | `CN8`/`CN9` boundary |
| bottom-right | (55.96, 73.57) | `CN7`/`CN10` boundary |

**This superseded an earlier, dropped attempt**: the first try measured
the 5 holes as a rectangle relative to *each other*, independent of the
connectors, and when placed absolutely it landed directly on `U1`'s real
Morpho pads — a genuine `hole_clearance` DRC error (an actual "can't
drill there" collision), not the already-expected courtyard-overlap kind.
Rather than paper over that by moving the holes to the board's corners
(which stopped being faithful to the drawing), `CN7`/`CN8` got re-measured
properly instead (see "Mechanical layout" above) so holes and headers
share one coordinate frame. With that fix, all 5 holes sit close to the
header boundaries — the same place they are on the real drawing — with no
real DRC collisions (confirmed: no `hole_clearance`, `solder_mask_bridge`,
or `hole_to_hole` violations; only the already-documented courtyard/NPTH
findings from sitting near `U1`'s courtyard).

Same calibration-uncertainty caveat as above applies — verify against the
physical board or UM2407 before drilling.

## LCD mounting (grounded in the module's own 2D drawing)

`Documentation/3.2inch_320x240_Touch_LCD_2D.pdf` ("HY32E-size.dwg") is a
fully-dimensioned mechanical drawing of the Waveshare 3.2" LCD module, so
the LCD's mounting holes and connector position are reproduced from it
rather than guessed:

- **4 mounting holes** (`MountingHole_3.2mm_M3`, no net — pure mechanical
  footprints) reproduce the LCD board's own hole pattern exactly: 79.00mm
  apart in X, 54.00mm apart in Y. Both spacings are explicit dimensions on
  the drawing (`79.00±0.1` and `54.00`), and `4.00 + 54.00 + 4.00 = 62.00`
  ties out exactly against the drawing's overall board height — strong
  confirmation these four numbers are the right ones to trust (the sheet
  has other dimensions that don't tie out as cleanly, and were not used).
  Mount the LCD on M3 standoffs through these holes.
- **`J1`** is positioned from the same anchor (the top-left hole): the
  drawing dimensions the connector's top pin as 6.67mm below the top-left
  hole, visually in line with it in X. So once the LCD is standoff-mounted
  at the 4 holes, its real P1/P2 connector should land directly on `J1` —
  that's the point of grounding both the holes and the connector in the
  same drawing instead of eyeballing them independently.
- A **reference outline** of the full LCD footprint (94.97 x 62.00mm) is
  drawn on the `Dwgs.User` layer (not `Edge.Cuts` — it's not part of this
  board's own cut shape, just a visual "the LCD sits here" reference,
  visible in the KiCad PCB editor but not in the quick renders above).
- **Confidence**: the hole pattern and overall outline are solid (multiple
  dimensions tie out exactly, see `gen_pcb.py`'s comments). The
  connector's X alignment to the hole column is a visual read of the
  drawing, not off an explicit dimension — worth confirming against the
  physical LCD board before final assembly. The drawing also confirms the
  LCD module exposes **male** pins (matching the female-socket assumption
  for `J1` below), which resolves half of Open Item 1.

## Pin-1 orientation fix (`CN7`-`CN10`, `CN11`, `CN12`, `J1`)

Caught by inspection against the mechanical drawings, not something the
earlier pin-mapping work checked: the generic KiCad "Odd_Even" connector
footprint's pad 1/2 column assignment doesn't match the real silkscreen on
either the Nucleo or the LCD module. Verified directly rather than just
asserted:

- **Nucleo Zio headers**: on our board, `U1` (Morpho/CN11) sits at
  X=8.25mm; `CN8` pad "1" was at X=15.73mm and pad "2" at X=13.19mm — pad
  2 is the column physically closer to the Morpho connector. But UM1974's
  drawing (zoomed on the real `CN8`/`CN11` silkscreen) shows pin 1 (the
  "NC" square pad) is the column **closer** to CN11. Same check on `CN7`
  against `CN12` confirmed the identical mismatch. So pads 1↔2 (and every
  odd/even pair: 3↔4, 5↔6, ...) were physically swapped on all four Zio
  headers (`CN7`-`CN10` all share the same footprint convention).
- **LCD header**: `J1` pad 1 landed on the *inner* column (toward the
  display) once placed at the measured LCD-hole-aligned origin, but the
  LCD's own 2D mechanical drawing
  (`Documentation/3.2inch_320x240_Touch_LCD_2D.pdf`) shows pin 1 on the
  *outer* column (toward the board edge) of its left-side header. Same
  mismatch, same fix.
- **Fix (final form)**: the footprints themselves are mirrored left-right
  in `gen_pcb.py` (`place_mirrored()`), not the net assignment — net
  numbering in `gen_sch.py` is the natural, un-swapped
  `pin_mapping.md`/`wiring_diagram.html` numbering. An earlier version of
  this fix swapped net assignments pairwise instead of touching the
  footprint, which is electrically equivalent but leaves the square
  "pin 1" silkscreen marker (geometry that belongs to the pad itself, not
  a label) sitting at the *wrong* physical location — still visually
  wrong when compared against the real board, which is what caught it.
  `FOOTPRINT.Mirror()` isn't usable from Python scripting (hits an
  unimplemented base-class virtual — `"Mirror used, should not occur"`),
  so `place_mirrored()` mirrors every pad and graphical item's local X
  coordinate by hand, about the *midpoint* between the two pad columns
  (mirroring about pad 1's own position was tried first and is wrong — it
  leaves pad 1 fixed and only moves everything else, not a real column
  swap; caught by checking the resulting pad positions numerically before
  trusting it). Re-verified against real pad positions after the fix:
  pad 1 is now the column closer to Morpho on all four Zio headers, and
  `J1` pad 1 (`FMC_D0`, the LCD's real physical pin 1) is now on the
  outer column, matching both mechanical drawings. ERC/DRC clean; the
  board was fully re-routed since net-to-pad geometry changed.
- **CN11/CN12**, added after the Morpho split above, use the exact same
  `place_mirrored()` fix as `CN7`-`CN10` — they're the same footprint
  family, so the same risk applies. Pin 1's real location (left column,
  confirmed from UM1974 Figure 6) was determined *before* placing them,
  not discovered as a bug afterward this time.
- **Not checked for this**: `J2`, `J5`, `J6`, `SW1`-`SW3` don't have this
  risk, since both ends of those connections are defined by this project,
  not an external fixed pinout to mismatch against.

**Second bug found and fixed while adding CN11/CN12**: `place_mirrored()`
mirrors pad/graphic positions by hand (see above), but was then calling
`fp.SetPosition(pos_mm)` directly — which moves the footprint's *anchor*
to `pos_mm`, not pad 1. Because the hand-mirror doesn't move the anchor
itself, pad 1 ends up one full pin pitch (2.54mm) away from `pos_mm`, not
at it. Caught by comparing `CN8`'s actual pad-1 position against
`CN8_OFFSET`'s intended target: they differed by exactly 2.54mm. This
affected every connector `place_mirrored()` had touched — `CN7`-`CN10`
and `J1` were each mechanically offset by one pin pitch from their
intended, pixel-measured positions (still on the *correct side*, since
mirroring direction was unaffected, just not at the *correct distance*).
Fixed by computing pad 1's post-mirror local offset from the anchor and
solving for the anchor position that puts pad 1 exactly at `pos_mm`.
Re-verified numerically after the fix: all six mirrored connectors'
(`CN7`, `CN8`, `CN9`, `CN10`, `CN11`, `CN12`) pad 1 now lands exactly on
its intended target coordinate, not offset.

## Routing

All 39 nets are routed (2 layers, 491 track segments, 27 vias, 0
unconnected items) using **Freerouting** (the standard open-source
autorouter the KiCad community uses for this — `kicad-cli` has no
autorouter of its own):

- `gen_pcb.py`'s board was exported to Specctra DSN (`pcbnew.ExportSpecctraDSN`)
  and routed with `freerouting-cli`/`freerouting.jar` v2.5.0, run by hand
  outside this repo (`-de F7CompressorShield.dsn -do F7CompressorShield.ses`).
  The native `freerouting-cli` binary hit a GraalVM native-image bug
  (`SerializationConstructorAccessor class not found`) partway through
  routing on this machine; the plain `.jar` run with a regular JVM worked.
- `pcbnew.ImportSpecctraSES()` (the normal way to bring a routed `.ses`
  back into KiCad) silently returned `False` here with no further error —
  possibly a limitation of this scripted/headless path rather than the
  interactive GUI import. Worked around it with a small custom importer
  (`routing/apply_ses.py`, not part of the KiCad project itself) that
  parses the `.ses` file's own S-expression routing data directly and
  creates the matching `PCB_TRACK`/`PCB_VIA` objects via the pcbnew
  Python API — net names, coordinates, and the via padstack size were all
  cross-checked against the DSN export before trusting the conversion.
- Freerouting's default track width (0.15mm) was below this board's
  declared minimum (0.2mm, netclass `Default`) and got flagged by DRC;
  every track narrower than 0.2mm was widened to exactly 0.2mm as part of
  the import step, which didn't introduce any new clearance violations.
- The route produced after the pin-1 mirror fix (above) left 3 genuine
  `clearance` violations (0.12-0.20mm actual vs. 0.2mm required) in two
  tight spots: a `BL_CTRL` via sitting too close to a diagonal `FMC_NE1`
  trace, and a short `BL_CTRL` dodge-around-`CN7`-pad-11 jog. Fixed by
  directly repositioning the via and the two dodge points with pcbnew
  scripting (not a full re-route) — computed from the actual track/pad
  geometry on the board, not guessed. The first via move (perpendicular
  to just the `FMC_NE1` trace) over-corrected into a *new* violation
  against an unrelated nearby `FMC_D15` trace, caught by re-running DRC
  immediately rather than trusting the hand calculation; the final
  position was solved for clearance against both tracks simultaneously.
  Re-verified clean (0 `clearance` findings, 0 unconnected) after.
- The re-route after the CN11/CN12 split (above) left 4 more `clearance`
  violations: a long `RELAY_IN3` trace (CN9 pin 4 to J2 pin 5) squeezed
  too close to both `CN10` pad 19 and a `J2` GND pad in the same tight
  spot (the two constraints left essentially zero margin for a single
  straight segment — a perpendicular nudge that cleared one made the
  other worse), and a short `GND` trace too close to a `CN8` pad. Fixed
  by computationally searching for a clear detour path, checking every
  candidate against *all* nearby pads **and tracks** (not just pads — an
  earlier instinct to check pads only is exactly why the first nudge
  attempt above over-corrected) before committing to it, then replacing
  only the exact flagged segments by precise coordinate match. First
  attempt at the `RELAY_IN3` detour used a bounding-box removal that
  over-matched an unrelated upstream segment and anchored the new path to
  the wrong endpoint (off by 0.14mm from the real one) — introduced
  dangling tracks, a crossing, and 2 unconnected items; caught by
  re-running DRC immediately rather than assuming the detour worked, and
  redone with exact segment identification instead. Re-verified clean (0
  `clearance`, 0 unconnected, no dangling/crossing) after.
- DRC after routing: down to 26 findings (fewer than before routing, since splitting the combined Morpho footprint into two correctly-sized CN11/CN12 connectors removed most of the old courtyard overlap with the Zio headers), all in the
  already-documented expected categories (courtyard overlap from the
  intentional Zio/Morpho/mounting-hole proximity, a couple of silkscreen
  warnings from tight stacking) — nothing routing-related, and
  `0 unconnected items`.

**FMC bus signal integrity — checked, neither controlled impedance nor
length matching is needed**: measured routed lengths range 45-156mm
(`FMC_D13` to `FMC_D2`). FMC pins are `GPIO_SPEED_FREQ_VERY_HIGH`
(`Core/Src/fmc.c`), whose datasheet worst-case rise/fall time is 3.3ns
(Table 54, speed `11`, 50pF load — realistically more like 30pF/2.4ns for
the actual trace+pad+ILI9341-input capacitance here). Against the FMC's
own 62.5ns data-setup window (15 AHB3 cycles @ 240MHz):
- Worst-case unterminated-trace ringing settles in ~5-7ns (a few round
  trips on the longest, ~150mm traces) — **~9x margin** before the bus is
  latched.
- Worst-case length-mismatch skew (~130mm, longest vs. shortest) is
  ~0.8ns — **~80x margin**.

Some of the longer nets (`FMC_A0`, `FMC_D2`/`D3`/`D14`, `FMC_NE1`/`NOE`/`NWE`)
exceed the standard "1/6 rule" critical-length threshold for a 3.3ns edge
(~92mm), so reflections are plausible on those specific traces, but with
that much timing margin they're not a functional concern — this is
standard, common practice for 8080-style parallel LCD interfaces at this
speed. If extra ringing/EMI margin mattered, the usual cheap fix is
22-33Ω series resistors at the MCU end of the longer lines; not pursued
here since it isn't required.

**Not done**: no manual cleanup pass on Freerouting's routing choices (via
placement, trace geometry) — it's exactly what the autorouter produced,
unreviewed.

## Open items (still unresolved — check before this goes near a fab house)

1. **Relay connector gender & pitch**: assumed 2.54mm male header for the
   relay module's J1 (module exposes a female header). Not verified
   against the actual board. (The LCD connector's gender is now confirmed
   male by its mechanical drawing — see "LCD mounting" above — so only the
   relay module side of this item remains open.)
2. **Mechanical layout precision**: see above — grounded in ST's real
   drawing now, but the inboard offsets between the Nucleo cluster's own
   connectors are an approximation, not a digitized measurement. Verify
   against UM2407 or the physical board before fab. (The LCD's mounting
   holes and connector position, by contrast, come from explicit
   dimensions that tie out exactly — see "LCD mounting" above.)
3. **Routing wasn't reviewed.** See "Routing" above — it's un-cleaned-up
   autorouter output, not hand-checked for sensible via placement or trace
   geometry around the FMC bus specifically.
4. No fabrication-readiness pass (DFM, silkscreen legend, copper pour, DRC
   rule exclusions for the expected overlaps above, etc.) has been done.
   This is a first-pass electrical + mechanical design
   check, not a send-to-fab file.

## Verification performed

```
kicad-cli sch erc  F7CompressorShield.kicad_sch   # 0 errors, 0 warnings
kicad-cli pcb drc  F7CompressorShield.kicad_pcb   # 0 unconnected, 26 findings, all in the
                                                   # expected courtyard/silk categories from the
                                                   # intentional Zio/Morpho overlap (see above)
```

Every net was also manually cross-checked pin-by-pin against the FMC, SPI5,
and Analog & Digital I/O tables in `../../wiring_diagram.html`.
