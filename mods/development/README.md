# Development-only mods

These packages are retained for development and are not staged by the game
build or release packagers.

`tomba2.debug.debug-menu` currently patches hot original game code without a
matching native overlay variant. The runtime correctly rejects the original
compiled bodies, causing severe interpreter slowdown even when resident
loading is disabled. The owner approved withholding it from the bundled
catalog while native support is deferred (`beads-eio.2.10`).

The authored cheat source and generated payload remain in `mods/sources` and
`src/mods`. Do not disable live-code guards to make those patches run as the
unmodified native code. Any future reintroduction needs native coverage of the
patched bodies and payload, plus frame-rate/audio validation.
