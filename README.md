# GT120 Sim (VST3)

Unofficial emulation of a British 120 W single-channel valve head (4 x EL34, 2 x ECC83) in the style of the
Matamp GT120. It is a behavioural model built from the few published facts about the amp, **not** a
schematic-exact copy, and is not affiliated with any manufacturer.

Published facts used: one channel; Volume, Bass, Treble, Presence, Bass Boost and a four-position Drive switch
placed after the first gain stage; presence circuit; 4 x EL34 class AB push-pull; silicon rectifier.
Everything else (gain staging, tone stack values, feedback amounts, sag) is an informed approximation.

## Signal flow
```
in -> V1a -> bass boost -> Drive (4 steps) -> V1b -> tone stack (Bass/Treble, fixed mid dip) -> Volume
   -> V2a -> phase inverter -> EL34 push-pull with negative feedback (Presence) + supply sag
   -> output transformer -> optional cabinet voicing -> out
```
There is no master volume, so Volume also sets how hard the power stage is driven, as on the real amp.

## Controls
| Control | What it does |
|---|---|
| Input | Host-side input trim, -24 to +24 dB (set it so your guitar level hits the amp like a real pickup would) |
| Drive | 4-position switch, increasing preamp gain |
| Bass Boost | Adds low-end weight before the drive stage |
| Volume | Level into the power amp (and so power-stage saturation) |
| Bass / Treble | Tone stack, with a fixed mid dip that deepens when both are low |
| Presence | Reduces high-frequency negative feedback: more bite and top end |
| Sag | Amount of supply sag under load (0 = stiff) |
| Cab | Simple guitar-cabinet voicing; turn it off if you use your own cab or IR |
| Output | Host-side trim, -24 to +12 dB |

Runs at 4x oversampling (the plugin reports the oversampler latency to the host).

## Build
CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/GT120Sim_artefacts/Release/VST3/GT120 Sim.vst3`

Install: Windows `C:\Program Files\Common Files\VST3\`, macOS `~/Library/Audio/Plug-Ins/VST3/`, Linux `~/.vst3/`.

No compiler? Push this folder to GitHub: `.github/workflows/build.yml` builds Windows, macOS and Linux VST3s as downloadable artifacts.

Offline test: `cmake -S . -B build -DGT_BUILD_TESTS=ON && cmake --build build --target dsp_test && ./build/dsp_test`

JUCE is used under AGPLv3/commercial licensing: if you distribute the plugin, AGPL terms apply.
