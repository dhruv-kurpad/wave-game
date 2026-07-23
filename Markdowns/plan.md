# Plan: Audio-Driven Wave Game

Phased build plan aligned with [spec.md](./spec.md). Each phase ends with a **done when** checklist. Prefer shipping a runnable slice at the end of every phase.

---

## Phase 0 — Project bootstrap

**Goal:** Empty app that builds, opens a window, and documents deps.

### Steps

1. Create repo layout from spec §10 (`src/audio`, `src/dsp`, `src/game`, `src/render`, `assets`, `config`).
2. Install toolchain deps (macOS): `cmake`, `pkg-config`, `portaudio`, `sdl2`, `sdl2_ttf` via Homebrew.
3. Add `CMakeLists.txt` linking PortAudio + SDL2 (+ SDL_ttf).
4. Vendor or fetch **KissFFT** (or equivalent) under `third_party/`.
5. Stub `main.cpp`: init SDL, clear screen, quit on Esc / window close.
6. Write `README.md`: install, configure, build, run, mic permissions on macOS.
7. Add `config/settings.json` with defaults (sample rate, frame size, wave points, sensitivities).

### Done when

- [x] `cmake -B build && cmake --build build` succeeds
- [x] Window opens and closes cleanly
- [x] README lists exact brew packages

---

## Phase 1 — Audio capture + ring buffer

**Goal:** Continuous PCM from the mic into a thread-safe buffer.

### Steps

1. Implement `RingBuffer` (SPSC float buffer; capacity ≥ 8–16× frame size).
2. Implement `AudioInput`: open default (or configured) input device, float32 @ 44.1 kHz, 1024 frames.
3. Callback only writes to ring buffer (no alloc, no DSP, no locks that can stall).
4. Add start/stop/teardown and error strings for no-device / permission failure.
5. Dev harness: console log of RMS every N callbacks **or** a debug bar in the SDL window reading buffer fill + RMS.
6. Stress-check: run 60+ seconds; confirm no sustained overflow (log drop count).

### Done when

- [x] Mic stream runs without crashing
- [x] DSP (or test consumer) can read samples continuously
- [x] Overflow/underrun is counted, not silent-fail forever

---

## Phase 2 — DSP core (volume mode first)

**Goal:** Amplitude → normalized value + wave height array.

### Steps

1. Add `DSPProcessor` thread (or timed pull from game loop initially; prefer dedicated thread per spec).
2. Volume path: RMS → gate → normalize → `volume_value` (`std::atomic<float>`).
3. Generate `wave_heights` (200–300 pts): map volume to base height + mild spatial variation.
4. Apply exponential smoothing (temporal) and neighbor smoothing (spatial).
5. Double-buffer wave heights; publish with brief mutex or atomic buffer index.
6. Wire config knobs: gate, sensitivity, smoothing alpha.
7. Visual smoke test: draw polyline of `wave_heights` reacting to speaking / silence.

### Done when

- [x] Speaking raises the wave; silence settles it
- [x] No audible/visible stutter from lock contention
- [x] Values stay in a stable 0–1 (or documented) range

---

## Phase 3 — DSP frequency mode

**Goal:** Dominant pitch drives control + wave.

### Steps

1. Integrate KissFFT; Hann window on analysis frames.
2. Magnitude spectrum; find dominant bin (ignore DC / very low bins).
3. Optional: parabolic peak interpolation for smoother Hz.
4. Map Hz → normalized height using bands: `<300` low, `300–1000` mid, `>1000` high.
5. Publish `pitch_value`; build `wave_heights` from mapped pitch and/or spectrum envelope.
6. Mode switch via atomic enum (UI can stay keyboard-only for now: `1` volume, `2` frequency).
7. Tune noise gate so quiet rooms do not jitter the peak bin.

### Done when

- [ ] Humming / whistling low→high visibly lifts the mapped height
- [ ] Mode switch changes control source without restarting audio
- [ ] Frequency mode usable without constant false peaks at silence

---

## Phase 4 — Water visualizer

**Goal:** Wave looks like a soft water surface, not a raw debug polyline.

### Steps

1. Implement Catmull-Rom (or similar) sampling across wave points for a dense render path.
2. Draw filled gradient under the crest to the bottom of the screen.
3. Soften crest (thicker stroke, alpha edge, or light glow).
4. Add slight horizontal phase oscillation for “water feel.”
5. Drive tint from `pitch_value` / `volume_value`.
6. Keep draw path ≤ frame budget at 60 FPS on target machine.

### Done when

- [ ] Wave reads as a continuous water surface
- [ ] Color responds to control signal
- [ ] Still 60 FPS with DSP running

---

## Phase 5 — Ball physics

**Goal:** Ball rests on the wave with gravity/buoyancy feel and damping.

### Steps

1. Implement `Ball` + `Wave::heightAt(x)` with interpolation.
2. Fix `ballX` at screen center; update `ballY` each frame.
3. Start with soft-follow (`targetY` blend); then add gravity + buoyancy + damping if needed.
4. Clamp speed; tune constants in config.
5. Detect crest contact for future particles (boolean / impulse magnitude).
6. Verify: sudden loud/pitch spikes do not teleport the ball unnaturally.

### Done when

- [ ] Ball sits on the surface at rest
- [ ] Raising control lifts ball; lowering drops it
- [ ] Jitter is acceptable (damped)

---

## Phase 6 — Gameplay shell

**Goal:** Mode select → play → score → collide → restart.

### Steps

1. State machine: `ModeSelect`, `Playing`, `Paused`, `GameOver`.
2. Mode select UI: Frequency / Volume buttons (mouse or keyboard).
3. Live wave preview on mode select (reuse renderer).
4. Implement scrolling pipe obstacles with randomized gap Y.
5. Collision AABB (or circle vs rect) between ball and pipes.
6. Score = distance traveled or pipes passed; HUD text via SDL_ttf.
7. Pause (Esc): freeze scroll; resume / quit to menu.
8. Game over screen: final score + replay / menu.

### Done when

- [ ] Full loop playable in both modes
- [ ] Collisions reliable; score updates
- [ ] Pause works without killing the audio device (or cleanly restarts)

---

## Phase 7 — Obstacle variety & difficulty

**Goal:** More patterns and scaling challenge.

### Steps

1. Add moving platforms and/or wave spikes (solid hitboxes).
2. Pattern picker with weights; avoid impossible gaps given ball speed limits.
3. Difficulty curve: scroll speed +, gap size −, spawn rate + by score.
4. Playtest both modes; adjust sensitivity defaults so neither is trivially easy/hard.

### Done when

- [ ] At least two obstacle types appear in a run
- [ ] Difficulty noticeably ramps
- [ ] No soft-lock impossible sections in early game

---

## Phase 8 — Polish & optional audio feedback

**Goal:** Feels like a finished mini-game.

### Steps

1. Pitch/volume meter widget on HUD.
2. Splash particles on ball–wave hits.
3. Optional spectrogram-style background from magnitude bins.
4. Optional SFX: slap tones, whoosh on fast Δy; quiet reactive bed music.
5. Calibration screen: silence floor + max level / pitch range.
6. Performance overlay toggle (FPS, DSP ms, ring fill %).
7. Config file load/save; document all keys.
8. (Stretch) Replay mode recording control inputs + RNG seed.

### Done when

- [ ] Meters + pause + score look intentional
- [ ] At least two visual polish features shipped
- [ ] Spec §13 MVP acceptance checklist is complete

---

## Suggested order & estimates

| Phase | Focus | Rough effort |
|-------|--------|--------------|
| 0 | Bootstrap | 0.5 day |
| 1 | Audio + ring buffer | 1 day |
| 2 | Volume DSP + debug wave | 1 day |
| 3 | Frequency DSP | 1–1.5 days |
| 4 | Water visuals | 1 day |
| 5 | Ball physics | 0.5–1 day |
| 6 | Gameplay loop | 1.5–2 days |
| 7 | Obstacles / difficulty | 1 day |
| 8 | Polish | 1–2 days |

**Critical path:** 0 → 1 → 2 → 5 (ball on volume wave) → 6, with 3–4 parallelizable after 2 once APIs stabilize.

---

## Working agreements

- Keep audio callback real-time safe.
- Prefer config over hardcoded magic numbers once a value is tuned twice.
- Each phase leaves `main` runnable; do not merge half-broken audio init.
- Validate against [spec.md](./spec.md) acceptance list before calling MVP done.

---

## Immediate next actions

1. Execute **Phase 0** (CMake + SDL window + folders).
2. Execute **Phase 1** (PortAudio + ring buffer + RMS debug).
3. Only then start Phase 2 DSP—do not build game obstacles before mic → wave is stable.
