# Test Cases by Phase

Test suite for [plan.md](./plan.md) / [spec.md](./spec.md).  
**Types:** `AUTO` = unit/integration (no mic), `MIC` = needs microphone, `MANUAL` = visual/playtest, `PERF` = timing/FPS.

**Conventions**

- ID format: `P{phase}-{category}{nn}` (e.g. `P1-RB03`)
- Categories: `BLD` build, `RB` ring buffer, `AUD` audio, `DSP` DSP, `WAV` wave, `RND` render, `BAL` ball, `GM` gameplay, `OBS` obstacles, `UI` UI, `POL` polish, `CFG` config
- Pass/fail against **Expected** only; notes go in a results log, not the case itself

---

## Phase 0 — Project bootstrap

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P0-BLD01 | AUTO | Clean configure | Homebrew deps installed | `cmake -B build -S .` | Configure succeeds; no missing package errors |
| P0-BLD02 | AUTO | Clean build | P0-BLD01 passed | `cmake --build build` | Binary builds with exit code 0 |
| P0-BLD03 | AUTO | Rebuild is incremental | Project already built | Touch `main.cpp`, rebuild | Relinks without full dependency reinstall |
| P0-BLD04 | AUTO | Directory layout | Repo checked out | List `src/{audio,dsp,game,render}`, `assets/`, `config/`, `third_party/` | All required dirs exist |
| P0-CFG01 | AUTO | Default settings file | — | Parse `config/settings.json` | Valid JSON; keys for sample rate, frame size, wave points, sensitivity present |
| P0-MAN01 | MANUAL | Window open/close | Built binary | Launch app; close via window X | Window appears; exits cleanly (code 0) |
| P0-MAN02 | MANUAL | Quit on Esc | App running | Press Esc | App exits without hang |
| P0-MAN03 | MANUAL | README accuracy | Fresh machine or clean env | Follow README brew + build steps exactly | Same success as P0-BLD01/02 |
| P0-BLD05 | AUTO | KissFFT present | — | Check `third_party/` for KissFFT (or documented FFT lib) | Sources or CMake fetch target exists |

---

## Phase 1 — Audio capture + ring buffer

### Ring buffer (unit — no mic)

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P1-RB01 | AUTO | Empty read | New buffer capacity N | `read` when empty | Returns 0 samples; no crash |
| P1-RB02 | AUTO | Write then read | Empty buffer | Write 1024 floats; read 1024 | Exact data match; available == 0 after |
| P1-RB03 | AUTO | Partial read | 2048 samples written | Read 512 twice | Order preserved; 1024 remain |
| P1-RB04 | AUTO | Wrap-around | Capacity 8192 | Write near end, wrap, read full sequence | Contiguous logical order correct |
| P1-RB05 | AUTO | Overflow drop policy | Buffer nearly full | Write more than free space | Oldest dropped or write truncated per design; overflow counter increments |
| P1-RB06 | AUTO | Capacity requirement | — | Construct with frame size 1024 | Capacity ≥ 8192 (8×) |
| P1-RB07 | AUTO | SPSC stress | Two threads | Producer writes frames 10s; consumer drains | No data corruption; overflow count logged if any |
| P1-RB08 | AUTO | No alloc in write path | Instrumented build optional | Call `write` in loop | Write path does not heap-allocate (policy check / comment verified in review) |

### Audio input

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P1-AUD01 | MIC | Start default device | Mic permission granted | `AudioInput::start()` | Stream running; callback fires |
| P1-AUD02 | MIC | Format & rate | Stream started | Inspect opened stream params | 44100 Hz, mono (or L channel), ~1024 frames |
| P1-AUD03 | MIC | Samples reach buffer | Stream + consumer | Speak into mic 3s; consumer reads | Non-near-zero RMS while speaking |
| P1-AUD04 | MIC | Stop/teardown | Running stream | `stop()` then destroy | No crash; callback stops; PortAudio cleaned up |
| P1-AUD05 | AUTO | No-device error | Mock/fail open or invalid device index | Start with bad device | Error string set; no uncaught exception; app stays alive |
| P1-AUD06 | MANUAL | Permission denied path | Deny mic in OS | Launch / start audio | User-visible error; no hang |
| P1-AUD07 | MIC | 60s soak | Stream + consumer | Run 60s idle room | No crash; overflow not sustained (avg fill stable) |
| P1-AUD08 | MIC | Overflow counter | Artificially slow/stop consumer briefly | Pause consumer 2s then resume | Overflow count increases; stream still alive after |
| P1-AUD09 | AUTO | Callback RT-safety review | Code review checklist | Inspect callback body | No `new`/`malloc`, no mutex that can block on game locks, no DSP/FFT |
| P1-AUD10 | MIC | Debug RMS harness | Dev build | Speak / silence | Console or on-screen RMS rises with voice, falls with silence |

---

## Phase 2 — DSP core (volume mode)

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P2-DSP01 | AUTO | RMS of silence | Buffer of zeros | Compute RMS | ≈ 0 |
| P2-DSP02 | AUTO | RMS of known tone | Synth 0.5 amp sine into buffer | Compute RMS | ≈ `0.5/√2` (±1%) |
| P2-DSP03 | AUTO | Gate below threshold | Samples below gate | Process volume path | `volume_value` ≈ 0 (or floor) |
| P2-DSP04 | AUTO | Normalize range | Amplitudes from 0→peak | Process | `volume_value` ∈ [0, 1] (or documented range) |
| P2-DSP05 | AUTO | Wave point count | Default config | After process | `wave_heights.size()` ∈ [200, 300] |
| P2-DSP06 | AUTO | Temporal smoothing | Step input 0→1 | Inspect successive frames | Output rises gradually (EMA), not in one frame to full |
| P2-DSP07 | AUTO | Spatial smoothing | Spike one index | Neighbor smooth | Adjacent points elevated; spike attenuated |
| P2-DSP08 | AUTO | Double-buffer publish | DSP + reader threads | Publish while reader copies | Reader never sees half-written / torn frame |
| P2-DSP09 | AUTO | Atomic volume read | Writer updates atomic | Reader samples | No data race (TSan clean if available) |
| P2-DSP10 | AUTO | Config knobs apply | Change gate/sensitivity/alpha in config | Reload or restart process | Behavior changes match new values |
| P2-MIC01 | MIC | Speak raises wave | Volume mode, live mic | Speak then silence | Wave / `volume_value` up then settles |
| P2-MAN01 | MANUAL | No lock stutter | App rendering wave | Speak continuously 30s | No visible hitching correlated with audio |
| P2-DSP11 | AUTO | Loud input clamp | Samples at full scale | Process | Heights and volume stay within max; no NaN/Inf |

---

## Phase 3 — DSP frequency mode

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P3-DSP01 | AUTO | Hann window energy | Constant signal | Apply Hann | Endpoints ~0; center peak |
| P3-DSP02 | AUTO | Dominant freq — 440 Hz | Synth 440 Hz @ 44.1 kHz, FFT size N | Detect peak | Reported Hz within ±(bin width) of 440 (tighter with interpolation) |
| P3-DSP03 | AUTO | Dominant freq — 200 Hz | Synth 200 Hz | Detect + map | Maps to **low** band (&lt;300) |
| P3-DSP04 | AUTO | Dominant freq — 600 Hz | Synth 600 Hz | Detect + map | Maps to **mid** band |
| P3-DSP05 | AUTO | Dominant freq — 1500 Hz | Synth 1500 Hz | Detect + map | Maps to **high** band |
| P3-DSP06 | AUTO | Ignore DC | DC offset + weak tone | Peak pick | Does not report ~0 Hz as dominant |
| P3-DSP07 | AUTO | Silence stability | Near-zero noise | Many frames | `pitch_value` stays low/stable; no random jumps to high |
| P3-DSP08 | AUTO | Mode switch atomic | Volume then Frequency | Flip mode enum mid-stream | DSP uses new path next frame; audio device not restarted |
| P3-DSP09 | AUTO | Pitch publish | Frequency mode | Process tone | `pitch_value` updates; wave heights non-flat vs silence |
| P3-MIC01 | MIC | Hum low→high | Frequency mode | Hum/whistle rising pitch | Mapped height / ball proxy rises |
| P3-MIC02 | MIC | Quiet room | Frequency mode, silent | Observe 10s | No constant false high peaks |
| P3-MAN01 | MANUAL | Keyboard mode toggle | Keys `1`/`2` wired | Toggle during preview | Wave control source changes immediately |

---

## Phase 4 — Water visualizer

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P4-WAV01 | AUTO | Catmull-Rom continuity | Known control points | Sample dense path | Samples pass through / smoothly between points; no NaN |
| P4-WAV02 | AUTO | Height-at bounds | Wave of W points | Query x=0, x=width, mid | Values within min/max of neighbors; no OOB crash |
| P4-RND01 | MANUAL | Filled surface look | Renderer on | Inspect under-wave fill | Gradient fill from crest to bottom visible |
| P4-RND02 | MANUAL | Soft crest | Renderer on | Inspect crest | Soft edge/glow; not 1px harsh polyline only |
| P4-RND03 | MANUAL | Horizontal oscillation | Static control signal | Watch 5s | Slight lateral water motion without changing mean height much |
| P4-RND04 | MANUAL | Tint follows signal | Raise volume/pitch | Observe color | Tint shifts with control value |
| P4-PERF01 | PERF | 60 FPS budget | DSP + wave render | Run 30s; log FPS | Average ≥ 55 FPS; 1% lows not &lt; 30 on target Mac |
| P4-RND05 | MANUAL | Full-width wave | Default window | Inspect | Wave spans screen; ~200–300 logical points |

---

## Phase 5 — Ball physics

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P5-BAL01 | AUTO | Rest on flat wave | Constant wave height H, ball radius R | Simulate until settle | `ballY ≈ H - R` (± epsilon) |
| P5-BAL02 | AUTO | Center X fixed | Any wave | Many frames | `ballX` remains screen center |
| P5-BAL03 | AUTO | Soft-follow lag | Instant wave step up | Measure ball Y | Ball approaches target over multiple frames (blend 0.2 or config) |
| P5-BAL04 | AUTO | Damping / no jitter | Small noisy wave (±ε) | Simulate | Ball Δy variance below threshold |
| P5-BAL05 | AUTO | Speed clamp | Huge wave jump | One frame update | `|vy|` ≤ configured max |
| P5-BAL06 | AUTO | heightAt interpolation | Linear ramp wave | Sample mid-segment | Interpolated value between adjacent samples |
| P5-BAL07 | AUTO | Contact flag | Ball crosses into crest | Update | Contact/impulse flag true when appropriate |
| P5-BAL08 | AUTO | No teleport on spike | Wave jumps 0→max in one DSP frame | Physics update | Ball displacement ≤ clamp; not instant snap to max |
| P5-MIC01 | MIC | Lift on raise | Ball + volume or pitch | Raise then lower control | Ball rises then falls |
| P5-MAN01 | MANUAL | Plausible motion | Play 30s | Watch ball | Looks “on water,” not vibrating violently |

---

## Phase 6 — Gameplay shell

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P6-GM01 | AUTO | Initial state | Fresh Game | Construct | State == `ModeSelect` |
| P6-GM02 | AUTO | Select → Playing | ModeSelect | Choose Frequency or Volume | State == `Playing`; mode matches choice |
| P6-GM03 | AUTO | Pause / resume | Playing | Pause then resume | Scroll frozen while paused; resumes from same positions |
| P6-GM04 | AUTO | Pause → menu | Paused | Quit to menu | State == `ModeSelect`; score reset policy applied |
| P6-GM05 | AUTO | Pipe scroll | Playing, fixed dt | Advance N frames | Obstacle X decreases by `speed * dt * N` |
| P6-GM06 | AUTO | Gap randomisation | Spawn many pipes | Collect gap Y | Gaps vary; always within screen + margin |
| P6-GM07 | AUTO | Collision hit | Ball AABB overlap pipe | Check collision | Collision true → transition `GameOver` |
| P6-GM08 | AUTO | Collision miss | Ball through gap | Check collision | No game over; score may increment |
| P6-GM09 | AUTO | Score increases | Survive scrolling | Pass pipes / travel distance | Score monotonic non-decreasing while Playing |
| P6-GM10 | AUTO | GameOver options | GameOver | Restart / menu actions | Restart → Playing; menu → ModeSelect |
| P6-GM11 | MIC | Audio survives pause | Mic running | Pause 10s, resume | Stream still delivering samples (or documented clean restart) |
| P6-MAN01 | MANUAL | Mode select preview | ModeSelect | Speak / hum | Live wave preview reacts |
| P6-MAN02 | MANUAL | Both modes full loop | — | Play Frequency and Volume to game over | Each mode completable; HUD score visible |
| P6-UI01 | MANUAL | HUD text | Playing | Observe | Score (and meters if present) readable via SDL_ttf |
| P6-GM12 | AUTO | Impossible soft state | Rapid pause spam | Toggle pause 50× | No deadlock; state always valid |

---

## Phase 7 — Obstacle variety & difficulty

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P7-OBS01 | AUTO | Multiple types spawn | Playing long enough | Record spawn types over 60s sim | ≥ 2 distinct types (pipes, platforms, spikes, …) |
| P7-OBS02 | AUTO | Hitboxes per type | Each obstacle type | Place ball overlapping solid | Collision registers for each type |
| P7-OBS03 | AUTO | Moving platform motion | Spawn moving platform | Advance time | Y (or X) follows motion function |
| P7-OBS04 | AUTO | Difficulty ramp — speed | Score 0 vs high score | Compare scroll speed | High-score speed &gt; early speed |
| P7-OBS05 | AUTO | Difficulty ramp — gap | Score 0 vs high | Compare gap size | Later gaps ≤ early (within config) |
| P7-OBS06 | AUTO | Early-game solvability | Score &lt; threshold | Generate 100 early patterns | Gap always reachable given max ball rise/fall rate |
| P7-OBS07 | AUTO | Pattern weights | Weighted picker | Large N spawns | Empirical frequencies ≈ configured weights (±tolerance) |
| P7-MAN01 | MANUAL | Noticeable ramp | Play one long run | Subjective | Game gets harder over time |
| P7-MIC01 | MIC | Both modes fair | Same difficulty config | Short runs each mode | Neither mode trivially impossible with default sensitivity |

---

## Phase 8 — Polish & optional audio feedback

| ID | Type | Title | Preconditions | Steps | Expected |
|----|------|-------|---------------|-------|----------|
| P8-UI01 | MANUAL | Pitch/volume meter | Playing | Change control signal | Meter tracks `pitch_value` / `volume_value` |
| P8-POL01 | MANUAL | Splash particles | Ball hits crest hard | Cause slap | Particles spawn and fade |
| P8-POL02 | MANUAL | Spectrogram bg (if shipped) | Frequency data available | Play tones | Background bands respond |
| P8-POL03 | MANUAL | SFX slap (if shipped) | Audio out enabled | Ball slap | Audible tone/click |
| P8-POL04 | MANUAL | Whoosh (if shipped) | Fast vertical move | Raise signal quickly | Whoosh plays |
| P8-UI02 | AUTO/MANUAL | Calibration | Calibration screen | Set silence + max | Subsequent normalize uses new floor/ceiling |
| P8-UI03 | MANUAL | Perf overlay toggle | Playing | Toggle overlay key | Shows FPS, DSP ms, ring fill; toggle off removes |
| P8-CFG01 | AUTO | Config round-trip | Change settings on disk | Load game | Values applied; invalid keys ignored or errored clearly |
| P8-CFG02 | AUTO | Documented keys | README / config comments | Diff vs actual loader | Every loaded key documented |
| P8-POL05 | AUTO | Replay stretch (if shipped) | Recorded seed + inputs | Replay | Deterministic obstacle pattern matches |
| P8-MVP01 | MANUAL | Spec §13 checklist | MVP build | Walk entire acceptance list in spec | All MVP boxes pass |

---

## Cross-phase regression pack

Run after each phase merge (subset OK if flagged N/A).

| ID | Type | Title | Expected |
|----|------|-------|----------|
| XR-01 | AUTO | Build still green | Configure + build succeed |
| XR-02 | AUTO | Ring buffer unit suite | All P1-RB* pass |
| XR-03 | AUTO | DSP silence/tone suite | P2-DSP01/02, P3-DSP02–05 pass |
| XR-04 | MIC | Mic smoke 10s | Capture + volume or pitch updates |
| XR-05 | MANUAL | Window quit paths | Esc and window close still work |
| XR-06 | PERF | Frame budget smoke | FPS not collapsed vs previous phase baseline |

---

## Suggested automation layout

```
tests/
  unit/
    test_ring_buffer.cpp
    test_rms.cpp
    test_fft_dominant.cpp
    test_wave_smooth.cpp
    test_ball_physics.cpp
    test_collision.cpp
    test_game_state.cpp
    test_difficulty.cpp
  fixtures/
    sine_440_f32.raw      # optional pregenerated
    silence_f32.raw
  manual/
    phase_checklists.md   # copy tables for playtest sign-off
```

**Harness notes**

- Prefer **GoogleTest** or **Catch2** linked in CMake `add_executable(wavegame_tests …)`.
- Synth PCM in-tests (sinusoids) so P2/P3 AUTO cases never need a mic.
- Gate `MIC` / `MANUAL` / `PERF` behind CTest labels: `ctest -L unit` vs `-L mic`.

---

## Exit criteria per phase

A phase is **test-complete** when:

1. All `AUTO` cases for that phase pass in CI/local.
2. All `MIC`/`MANUAL`/`PERF` cases for that phase signed off (or waived with reason).
3. Cross-phase regression pack run with no new failures.
