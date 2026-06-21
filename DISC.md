# Disc identity — Tomba! 2: The Evil Swine Return (USA)

Format: **bin/cue, two tracks (data + CD audio), MODE2/2352, NTSC-U**. Do **not**
convert to ISO — a 2048-byte "cooked" ISO discards the Mode-2 Form-2 XA sectors
PSX uses for streaming FMV/audio, and would also drop the Red Book audio track.

| Field | Value |
|-------|-------|
| Title | Tomba! 2 - The Evil Swine Return (USA) |
| Serial | SCUS-94454 |
| Track 01 | MODE2/2352, data |
| Track 02 | AUDIO (Red Book CD audio) |
| Size (.bin) | 438,483,360 bytes |
| MD5 | `c75678d1955e63181107ae1e8630d247` (locally computed) |
| SHA-1 | `b010e322dcfa8cb3aacaa4a940be2169fffafc6f` (locally computed) |

Hashes above are computed locally from the working dump; cross-check against the
Redump database before treating as canonical. Tomba! 2 (USA) shipped in a single
release.

Boot EXE: `SCUS_944.54` — load `0x80010000`, entry `0x80018B6C`, text `0x28800`
(165,888 bytes), initial `$sp` `0x801FFFF0`. `$gp` is 0 in the header (set at
runtime). The boot EXE is a small loader; the bulk of the game is streamed from
disc as overlays at runtime (same architecture as Tomba! 1).

Disc image and extracted EXE are local-only (gitignored); recreate from the
source dump if missing. Extract the boot EXE with mkpsxiso's `dumpsxiso`.
