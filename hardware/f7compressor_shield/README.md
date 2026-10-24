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
- **PCB (`.kicad_pcb`)**: all 12 footprints placed with correct net
  assignments. **Nothing is routed yet** — the 46 "unconnected items" DRC
  reports are the ratsnest for every net, expected at this stage.
- Two separate footprint clusters on this one board file:
  - **NUCLEO-mating cluster** (`U1`, `CN7`-`CN10`): laid out to mirror where
    these connectors actually sit under the real NUCLEO-144 board — see
    "Mechanical layout" below.
  - **Field-wiring cluster** (`J1`-`J5`, `R1`, `C1`): the LCD header, relay
    control header, and the 3 field screw terminals, placed well apart from
    the NUCLEO cluster since nothing about their position is constrained by
    the NUCLEO's own footprint.
- Reference designators: `CN7`–`CN10` are the NUCLEO Zio headers (female
  sockets, mate directly with the NUCLEO board), `U1` is the Morpho
  connector pair (CN11+CN12, see below), `J1` is the LCD module header,
  `J2` is the relay module's control header, `J3`–`J5` are 2-pin screw
  terminals (oil switch, e-stop, pressure loop), `R1`/`C1` are the pressure
  loop's 150Ω shunt and 100nF decoupling cap.

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

## What's routed vs. just present

Only the nets actually used by this design are wired — everything else on
the Zio/Morpho headers is populated (for mechanical fit) but left on
no-connect. See `CLAUDE.md` for the full signal-to-pin reference this was
built from.

## Morpho pad mapping (resolved)

Three FMC data lines (`PD8`/`PD9`/`PD10` = `FMC_D13`-`D15`) and the RUN
relay (`PG4`) are only reachable on the Morpho connectors (CN11/CN12),
which are unpopulated from the factory. Rather than guess at ST's official
Morpho footprint pad numbering, this was verified directly:

- KiCad ships an official `NUCLEO144-*` symbol family (`MCU_Module.kicad_sym`)
  built against the bundled `Module:ST_Morpho_Connector_144_STLink`
  footprint. The symbol's 144 numbered pins are real MCU GPIO names (same
  physical board/connector layout is shared across the whole STM32
  Nucleo-144 family, so the H743ZI variant's pin map applies unchanged to
  our H723ZG).
- Cross-checking against the 4 known Morpho signals from `CLAUDE.md`
  confirms the exact pad-numbering rule: **pad N = CN11 physical pin N for
  N 1-72, pad N = CN12 physical pin (N-72) for N 73-144.** (`PD9`→pad 69→
  CN11 pin 69 ✓, `PG4`→pad 141→CN12 pin 69 ✓, `PD8`→pad 82→CN12 pin 10 ✓,
  `PD10`→pad 137→CN12 pin 65 ✓ — all four match `CLAUDE.md` exactly.)
- The schematic instantiates `MCU_Module:NUCLEO144-F429ZI` directly (not
  the `H743ZI` variant, which only `extends` F429ZI and adds nothing of its
  own — instantiating it directly avoids a symbol-cache "extends" resolution
  issue that otherwise makes ERC silently drop all of U1's pins). The
  footprint pad numbers were confirmed to match the symbol pin numbers
  1:1 when building the PCB (every net resolved to a real pad, no
  mismatches).

This closes Open Item 1 from the original plan — the Morpho pad mapping is
no longer a guess.

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
  as the board-cluster frame.
- UM1974's drawing shows the Zio headers running in two stacked pairs
  flanking the MCU — `CN8` over `CN9` on the left edge, `CN7` over `CN10` on
  the right edge — each pair sitting inboard of the matching Morpho column.
  The drawing's 45.72mm/48.26mm dimensions (which differ by exactly one
  2.54mm pin pitch) read as the two pin columns of the right-side header,
  consistent with that header's own pitch.
- `U1` is placed at this frame's origin; `CN8`/`CN9` are placed ~7.6-10mm
  inboard of the CN11 columns, `CN7`/`CN10` mirrored inboard of CN12, each
  pair stacked directly on top of the other with a small gap — reproducing
  the real left/right, top/bottom arrangement.

**What's still approximate**: the ~5mm inboard offset and the vertical
stacking gap are a reasonable visual approximation, not digitized
pixel-for-pixel off the drawing, and this specific board (MB1364,
H723ZG) wasn't the exact revision pictured (UM1974 shows the original
MB1137 board) — ST keeps Zio/Morpho positions identical across the whole
Nucleo-144 family specifically so shields stay compatible across
revisions, which is why this is still a reasonable basis, but it's worth a
final check against UM2407 (the H7-specific manual) or the physical board
before fab.

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

## Open items (still unresolved — check before this goes near a fab house)

1. **LCD/relay connector gender & pitch**: assumed 2.54mm female socket for
   the LCD module's P1/P2 header (module exposes male pins) and 2.54mm
   male header for the relay module's J1 (module exposes a female header).
   Not verified against the actual boards.
2. **Mechanical layout precision**: see above — grounded in ST's real
   drawing now, but the inboard offsets are an approximation, not a
   digitized measurement. Verify against UM2407 or the physical board
   before fab.
3. **Routing.** Only placement + netlist exist; no copper traces.
4. No fabrication-readiness pass (DFM, silkscreen legend, mounting holes,
   copper pour, DRC rule exclusions for the expected overlaps above, etc.)
   has been done. This is a first-pass electrical + mechanical design
   check, not a send-to-fab file.

## Verification performed

```
kicad-cli sch erc  F7CompressorShield.kicad_sch   # 0 errors, 0 warnings
kicad-cli pcb drc  F7CompressorShield.kicad_pcb   # 46 unconnected (expected, unrouted) +
                                                   # expected courtyard/silk findings from the
                                                   # intentional Zio/Morpho overlap (see above)
```

Every net was also manually cross-checked pin-by-pin against the FMC, SPI5,
and Analog & Digital I/O tables in `../../wiring_diagram.html`.
