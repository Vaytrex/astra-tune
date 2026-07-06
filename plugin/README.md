# ASTRA-TUNE — Real-Time Pitch Corrector Plugin

Auto-Tune-style real-time pitch correction (YIN pitch detection + scale-snapped
delay-line pitch shifting) as a native audio plugin, built with JUCE.

Formats: **AAX (Pro Tools)**, AU, VST3, Standalone.

## Controls

| Control | Range | Notes |
|---|---|---|
| Key / Scale | 12 keys; Major, Natural Minor, Major/Minor Pentatonic, Chromatic | Notes snap to this scale |
| Retune Speed | 0–400 ms | **0 ms = hard tune** (the robotic "T-Pain" effect); 100–400 ms = transparent correction |
| Humanize | 0–100 % | Scales back the correction amount |
| Mix | 0–100 % | Dry/wet |
| Bypass | — | Also exposed as the host's master bypass |

Reported latency: ~15 ms (half the 30 ms shifter grain), so Pro Tools /
any host with delay compensation keeps the track in time.

## Build (AU / VST3 / Standalone — works today)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

JUCE is fetched automatically. On macOS the AU/VST3 are copied to
`~/Library/Audio/Plug-Ins/` after the build, so they show up in Logic,
GarageBand, Ableton, Reaper, etc. immediately.

## Installer

After building, create a macOS `.pkg` installer with selectable components:

```sh
installer/build-installer.sh
# -> build/ASTRA-TUNE-1.0.0.pkg
```

It packages whatever exists in the build artefacts — AU, VST3, the Standalone
app, and (once you've built with `-DAAX_SDK_PATH`) the AAX for Pro Tools,
installing to the system-wide plugin folders (`/Library/Audio/Plug-Ins/...`,
`/Library/Application Support/Avid/Audio/Plug-Ins`, `/Applications`).

The package is unsigned: fine for local installs (right-click → Open), but for
public distribution sign with a Developer ID Installer certificate and
notarize (`productsign` + `notarytool` — the script prints the exact commands).

## Building the AAX for Pro Tools

Pro Tools only loads AAX plugins, and Avid gates the AAX SDK behind a free
developer account. One-time setup:

1. Register at <https://developer.avid.com> (Avid Developer program, free tier
   covers AAX audio plugins).
2. Download the **AAX SDK** and unpack it anywhere.
3. Re-configure and build with the SDK path:

   ```sh
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DAAX_SDK_PATH=/path/to/aax-sdk
   cmake --build build -j8 --target AstraTune_AAX
   ```

4. Copy the result to the Pro Tools plugin folder:

   ```sh
   cp -R build/AstraTune_artefacts/Release/AAX/ASTRA-TUNE.aaxplugin \
     "/Library/Application Support/Avid/Audio/Plug-Ins/"
   ```

### Signing (the annoying-but-mandatory part)

- **Retail Pro Tools refuses unsigned AAX plugins.** For development, install
  the **Pro Tools Developer Build** (available with the Avid developer
  account) — it loads unsigned plugins.
- To run in normal/retail Pro Tools you must sign with **PACE Eden tools**
  (wraptool). PACE/Avid issue the signing certificates as part of the
  developer program; then:

  ```sh
  wraptool sign --account <acct> --wcguid <guid> \
    --in ASTRA-TUNE.aaxplugin --out ASTRA-TUNE.aaxplugin
  ```

- For distribution, register your own 4-char manufacturer code with Avid and
  replace `PLUGIN_MANUFACTURER_CODE`/`AAX_IDENTIFIER` in `CMakeLists.txt`.

## Architecture

- `Source/PluginProcessor.*` — audio engine: YIN pitch detection every ~11 ms
  on a 2x-decimated copy of the input; nearest-scale-note targeting; per-sample
  exponential glide (retune speed) driving a dual-tap crossfading delay-line
  pitch shifter. Stereo-safe (shared shift, per-channel delay lines).
- `Source/PluginEditor.*` — dark-panel UI with live note/frequency readout.
