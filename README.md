# ASTRA-TUNE

Real-time pitch correction, free and MIT licensed. Hard robotic tune or
invisible cleanup: YIN pitch detection plus a scale-snapped delay-line
pitch shifter.

**Website and live browser demo:** https://vaytrex.github.io/astra-tune/
**Download (macOS installer):** [ASTRA-TUNE-1.0.0.pkg](downloads/ASTRA-TUNE-1.0.0.pkg)

## What's in this repo

| Path | What it is |
|---|---|
| `/` (root) | The website, served by GitHub Pages. Includes the in-browser demo (AudioWorklet build of the engine). |
| `plugin/` | The native plugin source: JUCE/C++, builds AU, VST3, Standalone, and AAX (Pro Tools, needs the Avid SDK). See [plugin/README.md](plugin/README.md) for build and signing instructions. |
| `plugin/installer/` | macOS `.pkg` installer builder. |
| `web-demo/` | The standalone web version of the engine. |
| `downloads/` | The built installer served by the website. |

## Build the plugin

```sh
cd plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

JUCE is fetched automatically. AU/VST3 are copied to `~/Library/Audio/Plug-Ins/`
after the build. For Pro Tools (AAX) and installer packaging, see
[plugin/README.md](plugin/README.md).

## Support

If ASTRA-TUNE saved a take: [buy me a coffee](https://buymeacoffee.com/vaytrex).

MIT License. See [LICENSE](LICENSE).
