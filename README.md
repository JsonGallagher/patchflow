# PatchFlow

Node-based audio visualizer for macOS — inspired by Pure Data and TouchDesigner. Create patches by connecting nodes with cables. The graph processes live audio input (mic/system) to produce analysis signals (FFT, envelope, bands) that drive real-time GPU visuals (shaders, particles, waveforms, spectrograms).

## Prerequisites

- macOS 13+
- CMake (`brew install cmake`)
- Git (with submodule support)

## Build

```bash
# Clone with JUCE submodule
git clone --recursive https://github.com/JsonGallagher/patchflow.git
cd patchflow

# Configure + build
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(sysctl -n hw.ncpu)
```

## Run

```bash
open build/PatchFlow_artefacts/Debug/PatchFlow.app
```

macOS will prompt for microphone permission on first launch — grant it to get live audio input.

## Using the App

The window has 4 panels left-to-right: **Node Editor** | **Visual Output** | **Inspector**

### Basic Controls

| Action | Input |
|---|---|
| Add nodes | Right-click canvas → pick from category |
| Connect ports | Drag between compatible input/output ports |
| Rewire / disconnect | Drag from an input to a new output (or empty space to disconnect), or double-click a cable |
| Move nodes | Click and drag the node header |
| Zoom | `Cmd+Scroll` |
| Pan | `Space+Drag` |
| Delete | Select node, press `Backspace` |
| Undo / Redo | `Cmd+Z` / `Cmd+Shift+Z` |

### Port Types

| Color | Type | Description |
|---|---|---|
| Blue | Audio | Raw audio samples (block-rate) |
| Green | Signal | Scalar value (control-rate) |
| Orange | Buffer | Array data like FFT bins |
| Purple | Visual | Parameters for rendering (frame-rate) |
| Red | Texture | GPU texture handle |

Ports connect to the same type. Signal can also connect to Visual (implicit rate conversion).

### Load a Preset

Use the **Preset** dropdown in the top bar to load factory or user presets instantly.

Use the **Preset...** button for actions:
- **Refresh Presets**
- **Save As Preset...** (writes to user preset folder)
- **Update Selected Preset**
- **Reveal Selected Preset**
- **Delete Selected User Preset**

The old **File → Open** / **File → Save** flow still works for importing/exporting arbitrary JSON files.

Recommended starting points:
- `03_neon_glow.json` — pulsing neon sine waves driven by bass and highs
- `08_bass_nebula.json` — fractal noise nebula driven by low frequencies
- `10_particle_field.json` — multi-layer starfield, each band drives a layer

Play music or speak into your mic and watch the visual output react.

### Build a Patch from Scratch

A minimal reactive visual chain:

1. Add **Audio Input** (source node)
2. Add **FFT Analyzer** — connect `audio_L` → `in`
3. Add **Spectrum Renderer** — it auto-receives FFT data via the analysis pipeline
4. Add **Output Canvas** — connect `texture` → `texture`

For shader-based visuals, swap Spectrum Renderer for **Shader Visual** and connect analysis nodes (EnvelopeFollower, BandSplitter, Smoothing) to its `param1`–`param4` inputs. Edit the GLSL in the Inspector panel.

### Audio Settings

**File → Audio Settings** to select input device and buffer size.

## Node Reference

### Audio Source
- **Audio Input** — Live mic/system audio (stereo)

### Audio Processing
- **Gain** — Multiply audio by a gain factor
- **FFT Analyzer** — Windowed FFT producing magnitude spectrum and energy
- **Envelope Follower** — Track amplitude with attack/release
- **Band Splitter** — Split FFT into 5 bands (sub, low, mid, high, presence)
- **Smoothing (Lag)** — One-pole lowpass on signal values

### Math
- **Add** / **Multiply** — Combine two signals
- **Map Range** — Remap a value from one range to another
- **Clamp** — Constrain a value between min and max

### Visual
- **Color Map** — Map a 0–1 value to a color (heat, rainbow, grayscale, cool)
- **Waveform Renderer** — Render waveform to texture
- **Spectrum Renderer** — Render spectrum bars to texture
- **Shader Visual** — Custom GLSL fragment shader with audio-reactive uniforms
- **Output Canvas** — Final output to screen

## Architecture

Three-thread model:
- **Audio thread** (RT-safe) — processes audio nodes, writes analysis to lock-free FIFO
- **GUI thread** — owns graph model, compiles graph, publishes via atomic pointer swap
- **GL thread** — processes visual nodes at 60fps, composites to screen

## License

MIT
