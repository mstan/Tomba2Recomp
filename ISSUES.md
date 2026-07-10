# Tomba 2 Known Issues

## OpenGL renderer is substantially slower than software

Status: open. Tomba 2 defaults to the software renderer until this is fixed.

The OpenGL renderer makes both the Whoopee Camp logo/FMVs and regular gameplay
visibly sluggish. This is not an overlay compilation or static-dispatch miss,
and native-wide rendering is only a minority of the gameplay cost.

### Reproduction

1. Build the Release target with debug tools enabled.
2. Set `renderer = "opengl"`, `supersampling = 1`, and
   `texture_filtering = "nearest"` in the active settings.
3. Use either 4:3 or 16:9. The Whoopee Camp logo is slow in both; 16:9 also
   makes the gameplay cost easy to profile in the first unattended attract demo.
4. Launch with the Tomba 2 game config and a debug port:

   ```powershell
   .\Tomba2Recomp.exe --game ..\game.toml --no-launcher --debug-port 4615
   ```

5. Query the always-on measurements:

   ```powershell
   python ..\psxrecomp-v4\tools\raw_tcp.py 4615 frame_perf
   python ..\psxrecomp-v4\tools\raw_tcp.py 4615 latency window=240
   python ..\psxrecomp-v4\tools\raw_tcp.py 4615 overlay_loader_status
   python ..\psxrecomp-v4\tools\raw_tcp.py 4615 dispatch_stats
   python ..\psxrecomp-v4\tools\raw_tcp.py 4615 fmv_state
   ```

The first Beach Town attract demo starts without controller input. FMV skipping
may be enabled to reach it sooner, but note that auto-skip still executes the
guest MDEC decode and teardown path; it suppresses pacing, audio, and most
presents rather than deleting guest work.

### Measurements from 2026-07-10

Configuration: Release build, OpenGL, 1x supersampling, nearest filtering,
16:9 native-wide, warm overlay cache.

Whoopee/FMV sample (256 frames):

- Total: 60.250 ms/frame average.
- Emulation/CPU phase: 58.708 ms average.
- Scene GPU: 54.510 ms average, 3155.060 ms maximum.
- Present GPU: 5.741 ms average.
- Observed rate: roughly 16 FPS.

Steady Beach Town gameplay sample after transition frames aged out (256 wide
frames):

- Total: 28.808 ms/frame average, 56.365 ms maximum.
- Frame-period median: 30.211 ms; 95th percentile: 47.912 ms.
- Scene GPU: 20.714 ms average.
- Canonical scene: 17.019 ms average.
- Native-wide mirror: 3.694 ms average.
- Present GPU: 8.052 ms average.
- CPU upload/flush: 2.998 ms average.
- About 945 primitives and 110 textured batches per frame.
- Observed steady rate: roughly 33-35 FPS.

The software renderer previously measured about 17.9 ms/frame in the same
worktree and did not exhibit the severe logo slowdown, which is why it is now
the title default.

### Evidence excluding other causes

- `dispatch_stats`: `miss_total = 0`, `miss_unique = 0` during the gameplay run.
- `overlay_loader_status`: no unregistered functions; the warm cache loaded the
  expected native overlay fragments.
- The 16:9 mirror averaged only 3.694 ms. The canonical OpenGL scene plus the
  presentation path dominates, so disabling the Beach backdrop fix would not
  solve the slowdown.
- The logo is slow in 4:3, where native-wide is inactive.

### Investigation targets

1. Profile synchronization in the canonical OpenGL path. The reported
   emulation/CPU phase tracks scene-GPU cost closely, suggesting serialized GL
   work, timer-query waits, readbacks, upload flushes, or implicit driver sync.
2. Inspect the MDEC-to-OpenGL upload/display path. FMV frames have very few
   primitives but disproportionately high scene-GPU time and multi-second
   maxima.
3. Reduce textured batch count or state churn in gameplay. Beach Town averages
   roughly 110 batches for 945 primitives.
4. Attribute the approximately 8 ms OpenGL present cost independently of the
   3.7 ms native-wide mirror.
5. Use `gl_ws_ablate`, `gl_wide_fast`, `frame_perf`, and `gl_present_ring` for
   controlled A/B measurements. Do not infer performance from visible speed
   alone.

### Acceptance criteria

- Whoopee Camp and streamed FMVs run at full speed with OpenGL in 4:3 and 16:9.
- Steady Beach Town gameplay sustains the intended frame rate without large
  frame-time spikes.
- OpenGL is no slower than the software renderer by enough to cause frame-rate
  loss on the same machine and settings.
- The 4:3 canonical image and the validated 16:9/21:9 Beach Town backdrop remain
  visually correct.
- Static dispatch remains at zero misses and no unregistered overlay functions.
