# Spec: Audio-Driven Wave Game (C++)

## 1. Overview

A sound-controlled physics game where microphone input drives a water-like wave surface. A ball rests on the wave; the player raises or lowers pitch or volume to lift or drop the ball and clear scrolling obstacles.

**Modes**

| Mode | Control signal | Player intent |
|------|----------------|---------------|
| Frequency | Dominant pitch | Raise pitch → lift ball; lower pitch → drop ball |
| Volume | RMS loudness | Louder → lift ball; quieter → drop ball |

**Success criteria**

- Continuous mic capture with stable latency
- Wave visually tracks DSP output with smooth motion
- Ball stays physically plausible on the wave (no uncontrolled jitter)
- Two playable modes with score and Flappy-style obstacles
- Mode select, meters, pause, and basic polish

---

## 2. Architecture

### 2.1 Threads

| Thread | Responsibility |
|--------|----------------|
| Audio capture | PortAudio/RtAudio callback → PCM into ring buffer |
| DSP | Read samples → FFT or RMS → wave heights + pitch/volume |
| Game / render | Physics, obstacles, UI, SDL2/SFML draw |

### 2.2 Data flow

```
Microphone PCM → Ring buffer → DSP → wave height array
                                      → pitch_value / volume_value
                                           ↓
                                    Physics (ball)
                                           ↓
                                      Renderer
```

### 2.3 Shared data & sync

| Data | Type | Sync |
|------|------|------|
| PCM samples | Ring buffer | Lock-free or mutex |
| Wave heights | `std::vector<float>` (double-buffered) | Swap under brief lock or atomic index |
| Pitch / volume | `std::atomic<float>` | Lock-free reads on game thread |
| Control mode | Atomic enum / int | Set on UI, read on DSP |

**Constraints**

- Audio callback must not allocate, block, or do heavy DSP
- Game thread never blocks waiting on audio longer than one frame budget (~16 ms @ 60 FPS)

---

## 3. Audio input

### 3.1 Library & config

- Library: **PortAudio** (preferred) or RtAudio
- Sample rate: **44100 Hz**
- Channels: mono (or take left channel)
- Frame / callback size: **1024 samples**
- Format: float32 preferred; convert if device is int16

### 3.2 Required components

- Device open / start / stop / teardown
- Mic callback writing into ring buffer
- Thread-safe sample handoff to DSP
- Failure paths: no device, permission denied, underrun/overrun logging

### 3.3 Ring buffer

- Capacity ≥ several callback frames (e.g. 8192–16384 floats)
- Single producer (callback), single consumer (DSP)
- Drop or overwrite oldest on overflow; never block in callback

---

## 4. DSP

### 4.1 Common outputs

| Output | Type | Use |
|--------|------|-----|
| `pitch_value` | `float` (normalized 0–1 or Hz) | Frequency mode, UI meter, color |
| `volume_value` | `float` (normalized 0–1) | Volume mode, UI meter, color |
| `wave_heights` | `std::vector<float>` (~200–300) | Wave mesh / fill, ball sampling |

### 4.2 Mode A — Frequency (pitch)

**Pipeline**

1. Pull frame from ring buffer
2. Hann window
3. FFT (KissFFT or equivalent)
4. Magnitude spectrum
5. Dominant-frequency detection (peak bin, optional parabolic interpolation)
6. Map frequency → control height:
   - `< 300 Hz` → low
   - `300–1000 Hz` → mid
   - `> 1000 Hz` → high
7. Build / update `wave_heights` from spectrum and/or mapped height
8. Publish `pitch_value` and wave buffer

**Tuning knobs (config)**

- FFT size, hop size, sensitivity, frequency range clamps, noise gate

### 4.3 Mode B — Volume (amplitude)

**Pipeline**

1. Pull frame from ring buffer
2. RMS amplitude
3. Normalize (calibration peak or rolling max with decay)
4. Map amplitude → wave height
5. Publish `volume_value` and wave buffer

### 4.4 Wave height generation

- Length: **200–300** points spanning screen width
- Temporal smoothing: low-pass / exponential moving average
- Optional spatial smoothing across neighboring points
- Slight horizontal phase oscillation for water feel (render or post-DSP)

---

## 5. Graphics & wave visualizer

### 5.1 Library

- **SDL2** (+ SDL_ttf for UI text) or SFML

### 5.2 Wave representation

- Polyline / filled polygon from `wave_heights`
- Spatial interpolation: **Catmull-Rom** spline between samples
- Soft edges, gradient fill under surface
- Optional glow / soft edge blur

### 5.3 Visual style (baseline)

- Gradient fill under wave
- Soft silhouette at crest
- Subtle horizontal oscillation
- Color shift driven by pitch or volume
- Optional: spectrogram-like background, splash particles on ball–wave contact

### 5.4 Resolution / frame rate

- Target: **60 FPS**, windowed (e.g. 1280×720), resize optional later

---

## 6. Ball physics

### 6.1 Placement

- `ballX` = screen center (fixed X for v1)
- Sample wave at `ballX` (interpolated between wave points)
- Rest height: `waveHeightAt(ballX) - ballRadius`

### 6.2 Forces / integration (v1)

Minimal stable model (spec baseline):

```cpp
float targetY = waveHeightAt(ballX) - ballRadius;
ballY += (targetY - ballY) * 0.2f;  // smoothing / soft follow
```

**Fuller model (preferred when polish allows)**

- Gravity pulls down
- Buoyancy proportional to submersion vs wave height
- Damping to kill jitter
- Clamp max vertical speed

### 6.3 Requirements

- Wave sampling with interpolation
- Buoyancy (or equivalent soft follow)
- Damping
- Collision response for particle FX when contacting crest

---

## 7. Gameplay

### 7.1 Screens / states

| State | Behavior |
|-------|----------|
| Mode select | Buttons: Frequency, Volume; live mic wave preview |
| Calibration (optional polish) | Set silence floor / max loudness / pitch range |
| Playing | Wave + ball + obstacles + score |
| Paused | Freeze scroll; mute DSP publish optional |
| Game over | Show score; restart / return to mode select |

### 7.2 Core loop (Playing)

- Obstacles scroll horizontally (Flappy-style)
- Player lifts/drops ball via pitch or volume
- Score increases with distance / time survived
- Collision with obstacle solids → game over (or life loss if extended)

### 7.3 Obstacles

| Type | Description |
|------|-------------|
| Pipes with gaps | Vertical pair; pass through gap |
| Moving platforms | Horizontal shelves with vertical motion |
| Wave spikes | Tall local wave features or solid spikes |

Patterns should be randomized within difficulty bands.

### 7.4 Difficulty

- Scroll speed increases over time or by score
- Gap size / spike density scales (config-driven)

---

## 8. UI

| Element | Notes |
|---------|-------|
| Mode selector | Frequency / Volume |
| Pitch / volume meter | Real-time bar or numeric |
| Score | Distance or points |
| Pause menu | Resume, quit to menu, maybe sensitivity |
| Performance overlay (polish) | FPS, DSP ms, buffer fill |

---

## 9. Audio feedback (optional)

- Tone / click on ball–wave slap
- Whoosh on fast vertical motion
- Background music modulated by wave height

Not required for MVP; listed for polish phase.

---

## 10. Code structure

```
src/
  audio/
    AudioInput.hpp / .cpp
    RingBuffer.hpp
  dsp/
    DSPProcessor.hpp / .cpp
    FFT.hpp
  game/
    Game.hpp / .cpp
    Ball.hpp / .cpp
    Wave.hpp / .cpp
    Obstacle.hpp / .cpp
  render/
    Renderer.hpp / .cpp
  main.cpp
assets/
  fonts/
  textures/
config/
  settings.json   # or .ini — sensitivity, FFT size, etc.
```

---

## 11. Configuration

Expose at least:

- Mic device index / default
- Sample rate, frame size
- FFT size, hop, sensitivity
- Volume noise gate / normalize ceiling
- Wave point count, smoothing coefficients
- Ball radius, follow/damping factors
- Scroll speed, gap size, difficulty curve

---

## 12. Non-functional requirements

| Area | Requirement |
|------|-------------|
| Latency | Perceived control lag ideally < ~100 ms |
| Stability | No audio glitches under normal CPU load; DSP must keep up |
| Safety | Graceful mic failure; game can show error and return to menu |
| Portability | macOS first (Homebrew PortAudio + SDL2); Linux/Windows later if time |
| Secrets / privacy | Mic used only in-process; no recording to disk unless user opts in |

---

## 13. Acceptance (MVP)

- [ ] Mic opens and fills ring buffer
- [ ] Frequency mode maps pitch to ball height
- [ ] Volume mode maps RMS to ball height
- [ ] Wave renders as filled water surface with smoothing
- [ ] Ball rests on wave without wild jitter
- [ ] Mode select + live preview
- [ ] Scrolling pipe obstacles + score + collision game over
- [ ] Pause + meters
- [ ] Builds on macOS with documented install steps

---

## 14. Out of scope (MVP)

- Network multiplayer
- Mobile ports
- Full spectrogram editor / DAW features
- Replay recording (post-MVP polish)
- Custom shader language / engine abstraction beyond SDL2/SFML
