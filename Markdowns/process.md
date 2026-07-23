# Process Log — Wave Game

Step-by-step record of **everything** done on this project, in order.  
Update this file whenever work happens — builds, fixes, doc edits, failed attempts included.

**Companion docs (same folder):** [learning.md](./learning.md) (concepts & design decisions) · [spec.md](./spec.md) · [plan.md](./plan.md) · [tests.md](./tests.md)

**Standing instructions**

| Doc | Role |
|-----|------|
| `learning.md` | Teach concepts and explain design decisions as phases are built |
| `process.md` | Log every action taken (this file) — not just the happy path |

`README.md` stays at the **repo root** (how to build/run). Engineering/teaching docs live under `Markdowns/`.

---

## Session overview

| Step | What happened | Outcome |
|------|----------------|---------|
| A | User provided full multimedia eng spec | Scope understood |
| B | Started toward building the game | Paused — user wanted docs first |
| C | Wrote `spec.md` + `plan.md` | Spec + phased plan |
| D | Wrote `tests.md` | Per-phase test cases |
| E | Phase 0 implementation + `learning.md` | App builds and opens a window |
| F | Created `Markdowns/process.md` | Process log started |
| G | User moved docs into `Markdowns/`; links fixed; standing rules locked in | All markdowns colocated |
| H | Phase 1: RingBuffer + AudioInput + debug harness + unit tests | Mic → ring → RMS bars |
| I | Phase 2: DSPProcessor volume mode + wave draw + DSP tests | Mic → DSP → wave polyline |

---

## A — Ingest the project brief

**Input:** A structured plan for an audio-driven wave game in C++ (threads, PortAudio/RtAudio, KissFFT, SDL2/SFML, ball-on-wave physics, Flappy-style obstacles, two modes: frequency + volume).

**What we did:**

1. Read the full brief (architecture → audio → DSP → wave → ball → gameplay → polish).
2. Inspected the workspace — empty folder.
3. Checked the machine for tools (`cmake`, `pkg-config`, `brew`, `g++`) and libraries (PortAudio, SDL2).
4. Drafted an internal todo list for a full implementation.

**Then:** User redirected — do **not** build the whole game yet. Produce planning docs first.

---

## B — Write the engineering spec (`spec.md`)

**Goal:** Turn the chat brief into a stable requirements document.

**What we did:**

1. Created `spec.md` (originally at repo root; now `Markdowns/spec.md`).
2. Structured it like a multimedia eng spec:
   - Overview + modes table
   - Threads and data flow
   - Shared-data / sync rules
   - Audio (44.1 kHz, 1024 frames, ring buffer)
   - DSP Mode A (FFT / pitch) and Mode B (RMS / volume)
   - Wave visualizer requirements
   - Ball physics formulas
   - Gameplay states and obstacles
   - UI, optional SFX, code layout
   - Config knobs, NFRs, MVP acceptance, out-of-scope
3. Kept Guided-Link-style topics as clear section headings so any part can be expanded later.

**Why:** Build and test work need a single source of truth that does not drift with chat history.

---

## C — Write the phased plan (`plan.md`)

**Goal:** Break the spec into actionable phases with exit criteria.

**What we did:**

1. Created `plan.md` with phases **0–8**:
   - 0 Bootstrap  
   - 1 Audio + ring buffer  
   - 2 Volume DSP  
   - 3 Frequency DSP  
   - 4 Water visuals  
   - 5 Ball physics  
   - 6 Gameplay shell  
   - 7 Obstacles / difficulty  
   - 8 Polish  
2. For each phase: goal, numbered steps, **Done when** checklist.
3. Added effort estimates, critical path, working agreements, and “do Phase 0–1 before obstacles.”

**Why:** Prevents building pipes before the mic→wave pipeline exists.

---

## D — Write test cases (`tests.md`)

**Goal:** Define how each phase is verified.

**What we did:**

1. Created `tests.md` with case IDs (`P0-BLD01`, `P1-RB02`, …).
2. Tagged types: `AUTO`, `MIC`, `MANUAL`, `PERF`.
3. Covered unit ideas (ring buffer, RMS, FFT tones), mic soaks, playtests, and a cross-phase regression pack.
4. Suggested a future `tests/` layout + CTest labels.

**Why:** Phase “done” means tests pass / signed off, not “code exists.”

---

## E — Phase 0: bootstrap the runnable project

**Goal (from plan):** Empty app that builds, opens a window, documents deps. Teach via `learning.md`.

### E.1 — Dependencies

**What we did:**

1. Ran `brew install cmake pkg-config portaudio sdl2 sdl2_ttf`.
2. Brew hit a **permission error** mid-upgrade (Python cellar file).
3. Re-checked the machine — packages were already usable:
   - `cmake` 4.x  
   - `portaudio-2.0`  
   - `sdl2` / `SDL2_ttf` via Homebrew  
4. Did **not** block on fixing the whole Homebrew upgrade; proceeded with what was present.

### E.2 — Directory layout

**What we did:**

1. Created folders matching the spec:

   ```
   src/audio  src/dsp  src/game  src/render
   assets/fonts  assets/textures
   config
   third_party
   ```

2. Added `.gitkeep` files so empty dirs are tracked by Git.

**Why:** Layout mirrors `PCM → DSP → game → render` before any feature code lands.

### E.3 — Vendor KissFFT

**What we did:**

1. Tried downloading a release tarball — archive was invalid / wrong format.
2. Fell back to `git clone --depth 1 https://github.com/mborgerding/kissfft.git` into `third_party/kissfft`.
3. Removed nested `third_party/kissfft/.git` so it is vendored source, not a nested repo.
4. Confirmed `kiss_fft.h` / `kiss_fft.c` (and `kiss_fftr.*`) exist.

**Why vendor:** Tiny, stable, offline builds; only need a couple of C files for later FFT work.

### E.4 — Config defaults

**What we did:**

1. Wrote `config/settings.json` with sections: `audio`, `dsp`, `wave`, `ball`, `game`.
2. Filled defaults from the spec (44100 Hz, frame 1024, ~256 wave points, sensitivities, window 1280×720, etc.).
3. Did **not** add a JSON parser yet — file is schema + documentation for later phases.

### E.5 — CMake project

**What we did:**

1. Wrote root `CMakeLists.txt`:
   - C++17, export `compile_commands.json`
   - Static library target `kissfft` from `kiss_fft.c` + `kiss_fftr.c`
   - `pkg_check_modules` for PortAudio, SDL2, SDL2_ttf
   - Executable `wavegame` → `src/main.cpp`
   - Link KissFFT + the three system libs
   - Extra macOS frameworks for SDL/audio
   - Post-build copy of `config/settings.json` next to the binary
2. Added `.gitignore` for `build/`, IDE junk, etc.

### E.6 — Stub `main.cpp`

**What we did:**

1. Implemented minimal SDL2 loop:
   - `SDL_Init(VIDEO)`
   - Create 1280×720 window titled “Wave Game”
   - Accelerated renderer + VSync
   - Poll events; quit on window close or Esc
   - Clear to deep teal `(12, 48, 64)`
   - Destroy renderer/window; `SDL_Quit`
2. No mic, no DSP, no game objects — intentional Phase 0 scope.

### E.7 — First build (failed) → fix → rebuild

**What we did:**

1. Ran `cmake -B build -S .` — configure **succeeded** (all packages found).
2. Ran `cmake --build build` — **link failed**: `library 'portaudio' not found`.
3. Inspected `pkg-config --libs portaudio-2.0` — library lives under Homebrew Cellar; needs `-L…` path.
4. Patched `CMakeLists.txt` with `target_link_directories(… PORTAUDIO_LIBRARY_DIRS …)` (and SDL dirs).
5. Reconfigured and rebuilt — **succeeded**.
6. Verified:
   - `build/wavegame` exists  
   - `build/config/settings.json` copied  
   - `otool -L` shows PortAudio + SDL dylibs  
   - Smoke-launched the app for ~2 seconds, then killed it  

**Lesson recorded in `learning.md`:** Homebrew + pkg-config often needs explicit library directories on macOS.

### E.8 — Docs for Phase 0

**What we did:**

1. Wrote root `README.md` — brew packages table, build/run, mic permission notes for later, layout, phase status.
2. Wrote `learning.md` — Phase 0 teaching: CMake, vendoring, SDL vs PortAudio split, game loop, config-early, what we skipped, hands-on exercises.
3. Marked Phase 0 **Done when** boxes complete in `plan.md`.

---

## F — Process doc created

**What we did:**

1. Created folder `Markdowns/`.
2. Wrote `process.md` as a chronological process log (including failures and fixes).

**Original intent:** Keep process writing separate from root engineering docs. (Superseded by step G — all project markdowns now live here.)

---

## G — Colocate all markdowns under `Markdowns/`

**Trigger:** User moved `spec.md`, `plan.md`, `tests.md`, and `learning.md` into `Markdowns/` (alongside `process.md`). Confirmed standing rules:

- Keep teaching + design decisions in **`learning.md`** as phases are built  
- Report **everything** done in **`process.md`**

**What we did:**

1. Confirmed new layout via file search — root no longer has those four docs; `README.md` remains at repo root.
2. Fixed broken relative links:
   - `README.md` → points at `Markdowns/*.md`
   - `learning.md` → `../CMakeLists.txt`, `../src/main.cpp`, `../config/settings.json`
   - `learning.md` intro → points at `process.md` for the action log
   - `process.md` companion links → same-folder `./` paths
3. Left `plan.md` ↔ `spec.md` and `tests.md` ↔ plan/spec links as `./` (still valid inside `Markdowns/`).
4. Updated this file’s overview table, standing instructions, and tree snapshot.
5. First `Write` of this refreshed `process.md` failed in-tool (`old_string` error on a Write call); retried and succeeded.

**Why keep README at root:** Clone → open README is the universal convention for “how do I build this?” Engineering/teaching narrative stays in `Markdowns/`.

---

## Commands that matter (reproduce Phase 0)

```bash
# Dependencies (macOS)
brew install cmake pkg-config portaudio sdl2 sdl2_ttf

# Configure + build
cmake -B build -S .
cmake --build build

# Run
./build/wavegame
# Esc or close window to quit
```

---

## Current tree (after step G)

```
.
├── Markdowns/
│   ├── process.md      ← this log (append every session)
│   ├── learning.md     ← teach + design decisions
│   ├── spec.md
│   ├── plan.md
│   └── tests.md
├── CMakeLists.txt
├── README.md           ← stays at root
├── .gitignore
├── config/
│   └── settings.json
├── src/
│   ├── audio/
│   │   ├── RingBuffer.hpp
│   │   ├── AudioInput.hpp
│   │   └── AudioInput.cpp
│   ├── dsp/     (.gitkeep)
│   ├── game/    (.gitkeep)
│   ├── render/  (.gitkeep)
│   └── main.cpp
├── tests/
│   └── test_ring_buffer.cpp
├── assets/
│   ├── fonts/
│   └── textures/
├── third_party/
│   └── kissfft/            ← vendored
└── build/                  ← generated (gitignored)
    ├── wavegame
    └── config/settings.json
```

---

## H — Phase 1: audio capture + ring buffer

**Goal:** Continuous PCM from the mic into a thread-safe buffer; prove a consumer can drain it.

### H.1 — RingBuffer

**What we did:**

1. Added header-only [`src/audio/RingBuffer.hpp`](../src/audio/RingBuffer.hpp).
2. SPSC design: atomic `write_pos_` / `read_pos_`, pre-sized `std::vector<float>`.
3. `write` / `read` handle wrap-around with a two-part copy.
4. Overflow policy: write only what fits; increment `overflowCount` for the rest (no overwrite of read cursor).
5. Removed `src/audio/.gitkeep` (real files now present).

### H.2 — AudioInput (PortAudio)

**What we did:**

1. Added [`AudioInput.hpp`](../src/audio/AudioInput.hpp) / [`AudioInput.cpp`](../src/audio/AudioInput.cpp).
2. Lifecycle: `Pa_Initialize` → open → start → stop/close → `Pa_Terminate` in destructor.
3. Config defaults match `settings.json`: 44100 Hz, 1024 frames, ring 16384, device −1 (default).
4. Callback (`audioInputPaCallback`) only calls `handleCallback` → `ring_->write`.
5. Mono preferred; on `Pa_OpenStream` failure with multi-channel devices, retry stereo and keep left channel via stack temp (max 2048 frames).
6. Error strings for init/open/start/no-device; macOS mic hint on start failure.
7. Kept `portaudio.h` out of the public header (`void* stream_`).

### H.3 — Main harness

**What we did:**

1. Rewrote [`src/main.cpp`](../src/main.cpp) to start `AudioInput`, drain the ring every frame, compute RMS, draw fill + RMS bars, log every ~60 frames.
2. Red clear color if audio failed to start; teal if OK.
3. Window title set to `Wave Game — Phase 1 Audio`.

### H.4 — Build issues fixed

1. Nested `Config` with default arg `AudioInput(Config = {})` failed to compile under AppleClang (“default member initializer needed…”).
2. Fixed by adding `AudioInput()` delegating to `AudioInput(Config{})` — no default arg on the Config ctor.

### H.5 — Tests + CMake

**What we did:**

1. Added [`tests/test_ring_buffer.cpp`](../tests/test_ring_buffer.cpp) (P1-RB01–06).
2. Updated `CMakeLists.txt`: compile `AudioInput.cpp` into `wavegame`; add `test_ring_buffer` target.
3. Build succeeded; all ring buffer tests **PASS**.
4. Smoke-run `./build/wavegame` ~3s: printed `Audio started: 44100 Hz, 1 ch …` (mic permission OK in this environment).

### H.6 — Docs

**What we did:**

1. Wrote Phase 1 section in `learning.md` (ring buffers, RT-safety, atomics sketch, overflow policy, PortAudio lifecycle, harness).
2. Marked Phase 1 done in `plan.md`.
3. Updated `README.md` phase status + how to run tests.
4. Appended this section H.

**Phase 1 done when:** mic stream runs; consumer reads continuously; overflows counted — satisfied for MVP of the phase.

---

## I — Phase 2: volume DSP + wave heights

**Goal:** Amplitude → normalized volume + smoothed wave height array; render reacts to speech.

### I.1 — DSP math helpers

**What we did:**

1. Added [`src/dsp/DSPMath.hpp`](../src/dsp/DSPMath.hpp): `computeRms`, `volumeFromRms` (gate + peak envelope), `temporalSmooth`, `spatialSmooth`, `buildWaveFromVolume`.
2. Kept these header-only / inline so unit tests need no PortAudio or threads.

### I.2 — DSPProcessor thread

**What we did:**

1. Added [`DSPProcessor.hpp`](../src/dsp/DSPProcessor.hpp) / [`DSPProcessor.cpp`](../src/dsp/DSPProcessor.cpp).
2. `start(RingBuffer&)` launches `std::thread`; loop every ~5 ms: read up to `analysis_frames`, process or decay toward silence.
3. Publishes `std::atomic<float> volume_` / `raw_rms_`.
4. Double-buffers `wave_buffers_[2]` with `atomic published_` index (write back → flip).
5. Clamps `wave_point_count` to 200–300.
6. **Only DSP drains the ring** (main no longer reads PCM) — preserves SPSC.

### I.3 — Main visual harness

**What we did:**

1. Rewrote [`src/main.cpp`](../src/main.cpp): start audio + DSP; each frame `copyWaveHeights`, draw volume bar, filled polyline wave.
2. Title: `Wave Game — Phase 2 Volume DSP`.
3. Stop order: `dsp.stop()` then `audio.stop()`.

### I.4 — Tests + build

**What we did:**

1. Added [`tests/test_dsp_math.cpp`](../tests/test_dsp_math.cpp) (P2-DSP01–07, 11).
2. CMake: compile `DSPProcessor.cpp` into `wavegame`; add `test_dsp_math` target.
3. All ring + DSP tests **PASS**.
4. Smoke-run ~3s: `Audio + DSP started`, `vol=0`, `points=256`, `overflow=0`, quiet-room RMS ~2e-4 (gated to vol=0).

### I.5 — Docs

**What we did:**

1. Phase 2 section in `learning.md`.
2. Marked Phase 2 done in `plan.md`.
3. Updated `README.md`.
4. This section I.
5. Removed `src/dsp/.gitkeep`.

---

## Not done yet (by design)

| Item | Belongs to |
|------|------------|
| FFT / pitch mode | Phase 3 |
| Water visualizer polish (Catmull-Rom, glow) | Phase 4 |
| Ball physics | Phase 5 |
| Obstacles, score, menus | Phase 6–7 |
| Meters, particles, calibration | Phase 8 |
| JSON config loader | Later polish |

---

## Next process entry

When Phase 3 starts, append **section J**: KissFFT / Hann / dominant frequency, `pitch_value`, mode switch, learning notes.
