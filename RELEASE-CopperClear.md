# CopperClear 1.0 — release notes

Add selective copper area removal to the deHarro PCB-Gcode fork for EAGLE 7.7. Define areas on `tCopperClear` or `bCopperClear` and generate independent CNC milling files, including inward tool-radius compensation, alternating raster paths and a clockwise finishing contour with rounded reflex-corner transitions. Dedicated machining parameters are available in PCB-Gcode Setup. Legacy machine profiles are migrated without losing existing settings. The resulting G-code can be height-compensated and combined with other machining operations in OpenCNCPilot.

**Suggested tag:** `copperclear-v1.0` (base PCB-Gcode remains 3.7.0).

**Upgrade:** Back up the installation and run `RUN pcb-gcode-copperclear-upgrade` before opening the normal setup. See [README-CopperClear.md](README-CopperClear.md) for installation, usage, limitations and validation status.

## Maintainer notes (for a complete repository checkout)

This package contains seven replacement or new source files. It intentionally does not contain the maintainer's active configuration, saved machine profiles, board files, G-code or backup files. Apply the source files to the current local fork, preserving any additional local edits not included in the original archive.

To make a fresh checkout usable without a mandatory upgrade step, add these declarations to the repository's distributed settings headers, only if missing. The values below assume `OUTPUT_UNITS = U_MILLIMETERS`; convert lengths and feeds for other units. For an existing installation the upgrade ULP already performs this step, so users should not copy over their own settings.

`settings/pcb-defaults.h`:

```cpp
int GENERATE_TOP_COPPER_CLEAR = NO;
int GENERATE_BOTTOM_COPPER_CLEAR = NO;
real COPPER_CLEAR_DEPTH = -0.080000;
real COPPER_CLEAR_TOOL_SIZE = 1.400000;
real COPPER_CLEAR_OVERLAP = 50.000000;
```

`settings/pcb-machine.h`:

```cpp
real FEED_RATE_COPPER_CLEAR_XY = 300.000000;
real FEED_RATE_COPPER_CLEAR_Z = 100.000000;
real SPINDLE_COPPER_CLEAR_RPM = 10000.000000;
```

Retain existing copyright and license files. Link the CopperClear README from the main repository README. Review the diff and check that no personal settings or generated files are staged. Before tagging a final release, a physical cut with the final contour code is recommended, as well as a fresh-checkout setup test and, if feasible, a top-side test. The earlier physical test and V0.2b visual confirmation are documented separately in the README.
