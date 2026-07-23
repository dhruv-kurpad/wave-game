# Wave Game

Sound-controlled physics game in C++: microphone input drives a water-like wave; a ball rests on the surface. Two modes — **frequency** (pitch) and **volume** (loudness).

Docs live in [`Markdowns/`](./Markdowns/): [spec](./Markdowns/spec.md), [plan](./Markdowns/plan.md), [tests](./Markdowns/tests.md), [learning](./Markdowns/learning.md), [process](./Markdowns/process.md).

## Requirements (macOS)

```bash
brew install cmake pkg-config portaudio sdl2 sdl2_ttf
```

Exact packages used by this project:

| Package | Role |
|---------|------|
| `cmake` | Build system generator |
| `pkg-config` / `pkgconf` | Finds library flags for CMake |
| `portaudio` | Microphone PCM capture |
| `sdl2` | Window, input, 2D rendering |
| `sdl2_ttf` | HUD / menu text (later phases) |

Also needs: a C++17 compiler (`clang++` via Xcode Command Line Tools).

```bash
xcode-select --install   # if g++ / clang++ missing
```

## Build

```bash
cmake -B build -S .
cmake --build build
```

Binary: `build/wavegame`  
Config is copied to `build/config/settings.json` on each build.

## Run

```bash
./build/wavegame          # 1=volume, 2=frequency; Esc quits
./build/test_ring_buffer
./build/test_dsp_math
./build/test_pitch_fft
```

- Top bar = active control (amber volume / cyan frequency)
- Thin bars = volume + pitch meters
- Speak (mode 1) or hum/whistle low→high (mode 2)
- Red background = mic failed to start

### Microphone permission

macOS will prompt for Microphone access when audio capture starts.

- **System Settings → Privacy & Security → Microphone** — allow your terminal app (Terminal / iTerm / Cursor) if the stream fails
- Run from the same app you granted permission to

## Project layout

```
src/
  audio/     # PortAudio input, ring buffer
  dsp/       # FFT / RMS → wave heights
  game/      # Ball, obstacles, state machine
  render/    # SDL drawing
  main.cpp
assets/      # fonts, textures
config/      # settings.json
third_party/ # KissFFT (vendored)
```

## Phase status

| Phase | Status |
|-------|--------|
| 0 Bootstrap | Done |
| 1 Audio + ring buffer | Done |
| 2 Volume DSP + wave | Done |
| 3 Frequency DSP | Done |
| 4+ | Not started — see [plan.md](./Markdowns/plan.md) |
