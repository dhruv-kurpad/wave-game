# Learning Log — Wave Game

This file is the teaching companion to the build. Each phase explains **concepts**, **why we chose X over Y**, and **what to look at in the code**.

The step-by-step record of actions (including failed attempts) lives in [process.md](./process.md). Both files are updated as work proceeds.

---

## Phase 0 — Project bootstrap

**What we shipped:** a CMake project that links PortAudio + SDL2 + SDL_ttf + KissFFT, opens an SDL window, and quits cleanly. No mic or game logic yet — on purpose.

### Concept: why separate “bootstrap” from features?

In multimedia apps, the expensive bugs are often **environment** bugs: wrong library linked, wrong sample rate, missing frameworks on macOS. Phase 0 proves:

1. The compiler can see headers
2. The linker can find libraries
3. A window actually appears

Only then do we add real-time audio. If the window fails after audio is wired, you would not know which layer broke.

---

### Concept: subsystems as folders

```
src/audio   → “how do samples enter the program?”
src/dsp     → “what do those samples mean?”
src/game    → “rules, ball, obstacles, score”
src/render  → “pixels on screen”
```

**Design decision:** mirror the data flow from the spec (`PCM → DSP → physics → render`) in the folder tree.

Why not one big `src/` pile? Because this game is three different domains (I/O, signal processing, gameplay) that change at different rates. Keeping boundaries early makes it obvious where a new file belongs and keeps `#include` graphs sane.

Empty folders have `.gitkeep` so Git tracks them before code arrives.

---

### Concept: CMake (vs a hand-written Makefile)

**CMake** is a *generator*: you describe targets and dependencies; it produces platform-native build files (on macOS, often a Unix Makefile or Ninja file inside `build/`).

Why CMake for this project:

| Need | How CMake helps |
|------|------------------|
| Find PortAudio / SDL2 | `pkg_check_modules` + Homebrew paths |
| Mix C (KissFFT) + C++ | `LANGUAGES C CXX` |
| Out-of-source builds | `cmake -B build` keeps junk out of `src/` |
| Later: tests | Easy second target `wavegame_tests` |

**Design decision:** out-of-source build directory `build/`, listed in `.gitignore`. Never compile into the source tree — clean checkouts stay clean.

**Design decision:** C++17. We want `std::atomic`, structured bindings later, and wide compiler support without needing bleeding-edge C++20 for Phase 0.

Look at [`CMakeLists.txt`](../CMakeLists.txt):

- `add_library(kissfft STATIC …)` — third-party code is its own target
- `add_executable(wavegame …)` — our game links that library
- `pkg_check_modules` — asks pkg-config for include/lib flags
- `target_link_directories(… PORTAUDIO_LIBRARY_DIRS …)` — Homebrew’s PortAudio lives under Cellar; `-lportaudio` alone is not enough unless `-L…` is present. We hit “library 'portaudio' not found” once and fixed it by adding those dirs explicitly (a classic macOS + pkg-config footgun)
- macOS `*-framework …` — SDL/PortAudio ultimately need system frameworks; Homebrew’s `.pc` files sometimes omit them, so we add the common set explicitly

---

### Concept: vendoring KissFFT

**Vendoring** = copy the library into `third_party/` instead of `brew install kissfft`.

**Design decision: vendor KissFFT, brew for PortAudio/SDL2.**

| Library | Choice | Why |
|---------|--------|-----|
| PortAudio, SDL2 | Homebrew | Large, system-integrated, frequent security/platform updates |
| KissFFT | Vendored | Tiny, stable API, we only need 1–2 `.c` files; avoids “which FFT package?” on every machine |

KissFFT is deliberately small and dependency-free — ideal for teaching FFT without pulling FFTW’s complexity. We compile `kiss_fft.c` + `kiss_fftr.c` (`fftr` = real-input FFT, perfect for real PCM audio). We are **not calling it yet**; linking it in Phase 0 means Phase 3 cannot fail on “forgot to add FFT to the build.”

We deleted `third_party/kissfft/.git` so it is plain source in *this* repo, not a nested repository.

---

### Concept: SDL2 vs SFML (and vs using PortAudio for graphics)

The spec allowed SDL2 **or** SFML.

**Design decision: SDL2 for window/render/input, PortAudio for mic.**

- **SDL2** is the industry default for “I need a window and a game loop” tutorials and ships cleanly on macOS via Homebrew (`sdl2-compat` may provide `sdl2` — that is fine).
- **PortAudio** is purpose-built for low-latency device I/O. SDL can do audio too, but PortAudio’s callback + device model matches the spec’s audio thread story more closely.
- Splitting **capture** (PortAudio) from **display** (SDL2) reinforces the thread architecture you will build in Phase 1–2: audio must not hitch because the renderer stalled.

We link `sdl2_ttf` now even though Phase 0 draws no text — same rationale as KissFFT: prove the dependency once.

---

### Concept: the game loop (minimal)

Open [`src/main.cpp`](../src/main.cpp). The skeleton every realtime app shares:

```
initialize subsystems
while (running) {
  process input/events
  // update simulation   ← empty for now
  render a frame
}
shutdown subsystems
```

**Events:** SDL queues OS input (quit, keys). We `PollEvent` every frame so the window stays responsive. Esc and the window close button both set `running = false`.

**Renderer:** `SDL_RenderClear` + `SDL_RenderPresent` is “paint a color, flip to the screen.” `SDL_RENDERER_PRESENTVSYNC` syncs to the display refresh so we do not spin the CPU at thousands of FPS clearing teal.

**Design decision:** tear down in reverse order of creation (`Renderer` → `Window` → `SDL_Quit`). Resource leaks in a 10-second stub are harmless; habits matter once audio devices are open.

**Design decision:** placeholder clear color `(12, 48, 64)` — a deep teal so the “water game” mood is visible before any wave mesh exists. Pure black teaches nothing about art direction.

---

### Concept: config file before features

[`config/settings.json`](../config/settings.json) lists knobs we do not load yet (sample rate, FFT size, ball damping, …).

**Design decision: create the schema in Phase 0.**

Tuning DSP by editing constants scattered across `.cpp` files fights you. A single JSON document:

- Documents defaults next to the code
- Becomes the calibration surface in Phase 8
- Makes test cases (“set gate to X”) unambiguous

CMake **copies** `settings.json` next to the binary on build so running `./build/wavegame` can later load `./config/settings.json` relative to the executable. Phase 0 does not parse it yet — shipping the file early avoids “config was an afterthought.”

We are **not** adding a JSON library until a phase needs to read the file (keeps Phase 0 tiny).

---

### Concept: what we deliberately did *not* do

| Skipped | Reason |
|---------|--------|
| Parse settings.json | No feature needs it until audio/DSP |
| Open the microphone | Phase 1 — after the window is proven |
| Abstract “Engine” class | YAGNI; a clear `main` teaches more now |
| FetchContent for KissFFT every build | Vendored tree is offline-friendly and deterministic |

---

### Try this (hands-on)

1. Build and run:

   ```bash
   cmake -B build -S .
   cmake --build build
   ./build/wavegame
   ```

2. Change the clear color in `main.cpp`, rebuild, rerun — feel the edit → compile → see loop.

3. Open `build/compile_commands.json` (enabled via `CMAKE_EXPORT_COMPILE_COMMANDS`) — this is what clangd / Cursor use for accurate C++ navigation.

4. Run `otool -L build/wavegame | head` on macOS — see PortAudio and SDL dylibs listed even though Phase 0 code only calls SDL. Linking early is intentional.

---

### Phase 0 checklist (from the plan)

- [x] Folder layout per spec
- [x] Homebrew deps available (`cmake`, `pkg-config`, `portaudio`, `sdl2`, `sdl2_ttf`)
- [x] CMake links PortAudio + SDL2 + SDL_ttf + KissFFT
- [x] KissFFT under `third_party/`
- [x] Stub window; Esc / close to quit
- [x] README with brew packages + mic permission notes
- [x] `config/settings.json` defaults

**Next (Phase 1):** ring buffer + PortAudio callback — the first real-time constraint (the audio callback must never block or allocate).

---

## Phase 1 — Audio capture + ring buffer

**What we shipped:** mic PCM into a lock-free SPSC `RingBuffer`, drained by the game loop, with on-screen fill + RMS bars and a unit test binary for the buffer.

### Concept: why a ring buffer?

The audio device delivers samples on a **hard deadline** (every ~23 ms at 1024/44100). The game loop runs on vsync (~16 ms) and will later do physics and drawing. Those clocks are not synchronized.

A **ring buffer** (circular buffer) is a fixed-size array with a write cursor and a read cursor. When a cursor hits the end, it wraps to 0. That lets the producer keep writing without waiting for the consumer, as long as the consumer keeps up on average.

```
[####????####]  write wraps; read trails behind
     ^r    ^w
```

**Design decision: SPSC (single-producer / single-consumer).**  
One thread writes (PortAudio callback), one reads (main/DSP). That lets us use atomics on the cursors and **no mutex** in the callback. A mutex in the callback can priority-invert or block the audio thread → glitches / dropouts.

### Concept: realtime-safety in the callback

The PortAudio callback runs on an audio thread. Rules we follow (and will keep forever):

| Allowed | Forbidden |
|---------|-----------|
| Write into pre-allocated ring | `new` / `malloc` / `std::vector` grow |
| Simple math / copy | File I/O, locks shared with UI |
| Stack arrays of bounded size | Waiting on the game thread |

Look at [`src/audio/AudioInput.cpp`](../src/audio/AudioInput.cpp) `handleCallback`: stereo→mono uses a **stack** `float mono[2048]`, never the heap.

### Concept: acquire/release atomics (briefly)

`write_pos_` is stored with `memory_order_release` after data is copied; the reader loads it with `acquire` before reading slots. That pairs so the consumer never sees “new index” before the floats are visible. You do not need the full C++ memory-model textbook yet — remember: **release after publish, acquire before consume.**

### Concept: overflow policy

If the consumer is too slow, the ring fills. We **drop the samples that do not fit** and increment `overflowCount` (see `RingBuffer::write`).

**Design decision: drop-newest-that-do-not-fit (not overwrite-oldest).**  
Overwriting oldest requires the producer to advance the read cursor, which races with the consumer. Dropping excess writes keeps a pure SPSC protocol and makes overflows visible in the HUD/logs. For a game, if you overflow for long, you fix the consumer — you do not silently corrupt the read pointer.

Capacity **16384** ≈ 16 × 1024 frames → about 0.37 s of mono float at 44.1 kHz. That matches the spec’s “several callback frames” headroom.

### Concept: PortAudio stream lifecycle

```
Pa_Initialize → Pa_OpenStream → Pa_StartStream
        … callbacks fire …
Pa_StopStream → Pa_CloseStream → Pa_Terminate
```

**Design decision: `AudioInput` owns init/terminate.**  
Destructor stops the stream and calls `Pa_Terminate` if this instance initialized PortAudio. Phase 1 has one capture object; if we later add output, we may lift init to a process-wide guard — noted for later.

**Design decision: request mono, fall back to stereo.**  
Some macOS devices reject 1-channel input. We retry with 2 channels and keep the left channel only so DSP always sees mono.

### Concept: the game loop as consumer (Phase 1 harness)

[`src/main.cpp`](../src/main.cpp) each frame:

1. `read` until the ring is empty (into a pre-sized scratch vector)
2. Compute RMS = √(mean of squares) — a cheap loudness proxy
3. Draw **fill bar** (how full the ring is) and **RMS bar** (how loud)
4. Log once per ~60 frames: `rms`, `fill`, `overflow`

Fill should stay **low** when the consumer is healthy (we drain every frame). RMS should jump when you speak. Sustained rising overflow means the consumer is too slow (should not happen in Phase 1).

Window background turns **reddish** if `start()` failed (permissions / no device) so failure is visible without text rendering.

### Tests

[`tests/test_ring_buffer.cpp`](../tests/test_ring_buffer.cpp) covers P1-RB01–06 (empty read, round-trip, partial read, wrap, overflow counter, capacity). Run:

```bash
./build/test_ring_buffer
./build/wavegame   # speak; watch bars; Esc quits
```

### Design decisions summary

| Choice | Why |
|--------|-----|
| Header-only `RingBuffer` | Tiny, easy to test, no .cpp needed |
| Opaque `void* stream_` in the header | Keep `portaudio.h` out of the public header |
| Debug bars before a DSP thread | Prove capture+consume without inventing Phase 2 yet |
| Hardcoded audio config matching `settings.json` | Still no JSON parser; values stay in sync by convention |

### Phase 1 checklist

- [x] SPSC ring buffer with overflow counting
- [x] PortAudio capture @ 44.1 kHz / 1024 frames
- [x] Callback only writes (no alloc / DSP)
- [x] start/stop/teardown + error strings
- [x] RMS + fill debug harness (bars + console)
- [x] Unit tests for ring buffer

**Next (Phase 2):** dedicated DSP path — RMS → normalized `volume_value` + `wave_heights` array with smoothing (the consumer becomes smarter than “drain and meter”).

---

## Phase 2 — Volume DSP + wave heights

**What we shipped:** a dedicated DSP thread that drains the ring buffer, converts loudness into a normalized `volume` and a 256-point wave, and a render loop that draws a filled water polyline.

### Concept: three threads now

```
Audio callback  →  RingBuffer  →  DSP thread  →  wave + volume atomics
                                      ↓
                               Game / render thread (SDL)
```

Phase 1’s main loop was both consumer and renderer. That does not scale — physics and drawing will get heavier. **Design decision: DSP owns the ring consumer.** The render thread only *reads* published results (`volume()`, `copyWaveHeights`). One consumer keeps SPSC valid.

### Concept: RMS → gate → normalize

1. **RMS** (root-mean-square): √(mean of squares). For a sine of amplitude A, RMS ≈ A/√2. It is a stable loudness estimate.
2. **Gate**: if RMS < `volume_gate`, treat as silence (kills room-noise jitter).
3. **Normalize**: divide by a slow **peak envelope** so “your loud” maps near 1.0 without hardcoding mic gain. Envelope rises fast with loud peaks, decays slowly so volume stays usable.

Pure functions live in [`DSPMath.hpp`](../src/dsp/DSPMath.hpp) so tests do not need threads or a mic.

### Concept: temporal + spatial smoothing

Raw volume would make the wave twitch every DSP tick.

- **Temporal EMA:** `state = α·target + (1−α)·state` — low-pass over time (`smoothing_alpha` ≈ 0.2).
- **Spatial neighbor blend:** each point mixes with left/right neighbors (`spatial_smooth`) — kills single-point spikes so the silhouette looks like water, not noise.

### Concept: double-buffered wave publish

Two `std::vector<float>` buffers + `atomic<int> published_`:

1. DSP writes the **back** buffer completely  
2. Then stores the new index with `release`  
3. Render loads the index with `acquire` and copies that buffer  

**Design decision: atomic index flip, no mutex on the hot path.** The render thread may copy a wave one frame old — that is fine. It never sees a half-written vector as long as we assign the whole vector before flipping (vector assign is complete before `store`).

Caveat: `copyWaveHeights` does `out = src` which allocates if sizes differ; we keep sizes fixed at construction so after the first copy, SSO/capacity usually avoids churn. Phase 4 can switch to a fixed output span if needed.

### Concept: mapping volume → wave shape

`buildWaveFromVolume` sets a rest height (~0.22), lifts by `volume * 0.55`, and adds a small phase-scrolling ripple scaled by volume. Heights stay in **[0, 1]**; `main` maps 1 → higher on screen. Phase 4 will replace the strip-fill with Catmull-Rom + gradients.

### Config knobs (still hardcoded, matching `settings.json`)

| Knob | Role |
|------|------|
| `volume_gate` | Silence threshold |
| `volume_sensitivity` | Multiplier after normalize |
| `smoothing_alpha` | Temporal EMA |
| `spatial_smooth` | Neighbor blend |
| `wave_point_count` | Clamped to 200–300 |

### Try this

```bash
./build/test_dsp_math
./build/wavegame   # speak → wave rises; quiet → settles
```

Amber bar = normalized volume. Teal fill = wave under the crest.

### Phase 2 checklist

- [x] DSP thread drains ring buffer
- [x] RMS → gate → normalize → `volume` atomic
- [x] `wave_heights` ~256 pts with temporal + spatial smooth
- [x] Double-buffer publish
- [x] Config knobs wired in `DSPProcessor::Config`
- [x] Polyline + fill smoke visual
- [x] Unit tests for DSP math helpers

**Next (Phase 3):** frequency mode — Hann + KissFFT + dominant bin → `pitch_value`, mode switch without restarting audio.

---

## Phase 3 — Frequency DSP (pitch mode)

**What we shipped:** KissFFT-based dominant-pitch detection, a normalized `pitch` control mapped through low/mid/high bands, keyboard mode switch (`1` volume / `2` frequency) without restarting the mic, and gates so silence does not spam false peaks.

### Concept: what an FFT gives you

A block of time-domain samples (e.g. 1024 floats) can be rewritten as a sum of sinusoids. The **FFT** computes the amplitude (and phase) of each frequency **bin**.

- Bin width ≈ `sample_rate / nfft` → at 44100/1024 ≈ **43 Hz** per bin  
- Real signals use a **real FFT** (`kiss_fftr`): output length `nfft/2+1` (0 Hz … Nyquist)

We only need **magnitudes** `|X[k]| = √(re²+im²)` for “which pitch is loudest?”

### Concept: Hann window

Chopping audio into a finite block is like multiplying by a rectangle — that smears energy across bins (**spectral leakage**). A **Hann window** tapers the edges to ~0:

`w[i] = 0.5 * (1 - cos(2π i / (N-1)))`

**Design decision: always window before FFT.** Without it, a pure 440 Hz tone leaks into neighbors and peak-picking gets messier.

### Concept: dominant bin + parabolic refinement

1. Skip **DC** (bin 0) and bins below `pitch_min_hz`  
2. Find `k` with max magnitude in the allowed range  
3. **Parabolic interpolation** using `mag[k-1], mag[k], mag[k+1]` estimates a fractional bin → smoother Hz than “snap to 43 Hz steps”

### Concept: Hz → game control bands

Per the spec:

| Hz | Band | Unit range (approx) |
|----|------|---------------------|
| &lt; 300 | low | 0 … ⅓ |
| 300–1000 | mid | ⅓ … ⅔ |
| &gt; 1000 | high | ⅔ … 1 |

`mapPitchHzToUnit` piece-wise lerps through those edges up to `pitch_max_hz` (2000). Humming low→high should lift the wave; whistling high sits near the top.

### Concept: silence gate (false peaks)

In a quiet room the spectrum is tiny noise — some bin always “wins.” We require:

1. RMS ≥ `volume_gate` (there is actual energy)  
2. Peak magnitude ≫ spectrum mean (`pitch_peak_ratio`, default 8×)  
3. Absolute floor on peak mag  

Otherwise pitch **decays** toward 0 instead of jumping. That is why frequency mode stays calm when you are silent.

### Concept: mode switch without touching audio

`std::atomic<int> mode_` — UI stores `Volume` or `Frequency`; DSP reads it each block. **PortAudio keeps running.** Both volume and pitch are computed every block so the secondary meters stay live; only `controlValue()` (which drives the wave) depends on mode.

Keys in [`main.cpp`](../src/main.cpp): **`1`** volume, **`2`** frequency.

### Files to read

| File | Role |
|------|------|
| [`PitchMath.hpp`](../src/dsp/PitchMath.hpp) | Hann, peak pick, Hz→unit map |
| [`FFT.hpp`](../src/dsp/FFT.hpp) / [`FFT.cpp`](../src/dsp/FFT.cpp) | KissFFT RAII wrapper |
| [`DSPProcessor.cpp`](../src/dsp/DSPProcessor.cpp) | Mode + gates + wave publish |
| [`tests/test_pitch_fft.cpp`](../tests/test_pitch_fft.cpp) | Synth tones, no mic |

### Try this

```bash
./build/test_pitch_fft
./build/wavegame
# 1 = volume (speak), 2 = frequency (hum/whistle low→high)
```

Top bar = active control (amber volume / cyan frequency). Thin bars below = both meters.

### Phase 3 checklist

- [x] KissFFT + Hann window
- [x] Magnitude spectrum + dominant bin (+ parabolic Hz)
- [x] Band map &lt;300 / 300–1000 / &gt;1000 → [0,1]
- [x] `pitch` / `pitchHz` published
- [x] Mode switch via atomic (`1`/`2`), audio uninterrupted
- [x] Silence gating against false peaks
- [x] Unit tests with synthetic tones

**Next (Phase 4):** Catmull-Rom water surface, gradient fill, glow, tint from control — make it look like water, not a debug polyline.

---

## Phase 4+

*(written when we build it)*
