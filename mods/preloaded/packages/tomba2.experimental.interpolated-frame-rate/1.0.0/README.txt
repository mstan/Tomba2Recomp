Tomba 2 Temporal Frame Blending (Experimental)

This mod leaves Tomba 2's executable, guest VBlank, simulation, timers, input,
and audio untouched. It combines the two most recent completed game frames in
PSXrecomp's OpenGL presentation path at the display refresh or a fixed 60, 120,
144, or 165 presentation rate.

The motion-adaptive clarity blend avoids crossfading large pixel changes to
reduce double-image trails. It uses the same zero-sentinel and blend-mode fixes
as Ape Escape. This is temporal blending, not motion-vector frame generation, so
it cannot reconstruct true in-between object positions.
