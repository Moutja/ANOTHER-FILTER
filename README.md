# Another Filter (VST3, Windows)

An MS-20-style filter effect: **HP → LP**, each a 12 dB/oct Sallen-Key-style (Korg-35 topology)
stage with a tanh-saturated resonance feedback path, plus input drive, dry/wet mix and output trim.
This is an original implementation, not a copy of any existing plugin.

## Parameters
| Name | Range | Default |
|---|---|---|
| HP Freq | 20 Hz – 10 kHz (log) | 20 Hz |
| HP Peak | 0 – 100 % (self-oscillates at the top) | 0 |
| LP Freq | 20 Hz – 20 kHz (log) | 20 kHz |
| LP Peak | 0 – 100 % | 0 |
| Drive | 0 – 100 % (tanh input gain 1x – 8x) | 0 |
| Mix | 0 – 100 % | 100 |
| Output | -24 – +12 dB | 0 |

## Build (Windows)
Requirements: Visual Studio 2022 (Desktop C++ workload), CMake 3.20+, Git.

```
git clone --recursive https://github.com/steinbergmedia/vst3sdk.git C:\dev\vst3sdk
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_SDK_ROOT=C:\dev\vst3sdk
cmake --build build --config Release
```
The plugin lands in `build\VST3\Release\AnotherFilter.vst3`. Copy it to
`C:\Program Files\Common Files\VST3\` (admin) or load it from a custom VST3 scan folder.

If the linker reports unresolved `InitModule` / `DeinitModule` (older SDK versions), add
`bool InitModule() { return true; }` and `bool DeinitModule() { return true; }` to `ms20_entry.cpp`.

## Layout
- `source/ms20_filter.h` – all DSP (header-only, no SDK dependency, easy to unit test)
- `source/ms20_processor.*` – audio processor, parameter handling, state save/load
- `source/ms20_controller.*` – parameter definitions and editor creation
- `resource/ms20.uidesc` – the plugin window layout (VSTGUI XML: colors, fonts, knobs, readouts). Edit this file to restyle the UI; no C++ changes needed
- `resource/knob.png`, `knob_2.0x.png` – knob filmstrips (101 frames). Regenerate with `python tools/make_knob.py` (needs Pillow) after changing colors there
- `source/ms20_params.h` – ranges and defaults shared by processor and controller
- `source/ms20_ids.h` – GUIDs (replace them) and parameter IDs

## Known limits / next steps
- No oversampling yet, so heavy drive and high resonance will alias; 2x–4x oversampling around the
  filter is the first upgrade.
- The saturator uses the linear solution of the feedback loop to drive tanh (a standard approximation).
- UI is a first pass (vector knobs, fixed size). If the window is blank, check that
  `AnotherFilter.vst3/Contents/Resources/ms20.uidesc` and `knob.png` exist in the build output.
- Stereo only, 32-bit float only.

## Build with GitHub Actions (no local setup)
1. Create a GitHub repo and push this folder (including `.github/`).
2. Open the **Actions** tab, pick "Build VST3 (Windows)", and run it (it also runs on every push).
3. When it finishes, download the `MS20Filter-vst3-windows` artifact. Unzip it and copy the
   `AnotherFilter.vst3` folder to `C:\Program Files\Common Files\VST3\`.
