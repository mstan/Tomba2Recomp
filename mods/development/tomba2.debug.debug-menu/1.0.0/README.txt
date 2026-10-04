Tomba 2 Debug Menu
==================

This development package is excluded from runtime and release bundles. Its
patched render loop lacks matching native coverage and causes severe slowdown
even with Resident Loading disabled. Native support is deferred; see
mods/development/README.md and beads-eio.2.10 before reintroducing it.

An in-game developer menu drawn with Tomba 2's own text routines. Press L3
during gameplay to open it; X selects, O goes back, Up/Down move the cursor,
L3 closes.

Pages
-----
  FREE POSITION    move Tomba freely in X/Y, with L2 held for Z; hide objects
  WARP TOOLS       pick a destination area/entry and launch the transition
  EVENT FLAGS      step through the event-flag table and edit values
  SYSTEM TOOLS     freeze Tomba, freeze actors/objects, disable the effect
                   list, change game speed and render path, restore defaults
  INVENTORY TOOLS  grant every item
  WORLD INFO       live area, sub-area, level variable, player mode and state
  CONTROLS / HELP  the button reference

How it is installed
-------------------
The menu is guest MIPS code. When the feature is enabled and a gameplay module
is resident, the runtime writes the payload into free BIOS kernel RAM at
0x8000C000 and re-points five call sites inside the loaded gameplay overlay at
it. Every site is checked against its expected stock instruction first; if any
site does not match, nothing is patched and the reason is written to the log.
Because those sites live in overlay code that is re-streamed from disc on area
transitions, the runtime re-asserts them each frame while the feature is on.

Disabling the feature removes the plugin from the resolved plan entirely, so no
guest memory is touched and the game runs stock.

Warnings
--------
This is a development tool, not a gameplay enhancement. It is disabled by
default. Warping to a raw
destination, freeing the player position, and granting items can all put the
game into states the stock title never reaches, and those states can be saved.
Use a memory card you do not mind losing.

The menu's toggle is L3. Tomba 2 is pinned to a digital pad, and a real
SCPH-1080 digital controller has no L3 button; the runtime still delivers the
L3 bit, so the binding works here but does not correspond to anything you could
press on original hardware.

Credits
-------
The debug menu itself -- the MIPS payload, its pages, and its string table -- was
written by Discord user unicorngoulash, on behalf of the Tomba Club community,
and distributed as the "Debug Menu (Press L3 to toggle)" code list for Tomba! 2
(NTSC-U, SCUS-94454). It is their work; this repository only carries it.

The psxrecomp side is the wrapper: mods/sources/tomba2_debug_menu.cht holds the
code list verbatim, tools/gen_debug_menu_payload.py transcribes it into a C
header, and src/mods/tomba2_debug_menu_plugin.c installs it through the trusted
mod API with expected-value guards. Two adaptations were needed because the
original targets DuckStation with a retail BIOS:

  * The code list's own guard reads a game pointer out of BIOS kernel RAM at
    0x8000B080. That address is retail-SCPH1001 kernel layout; under the
    OpenBIOS kernel this runtime ships, the same pointer lives at 0x80008558 and
    0x8000B080 stays zero, so the original guard never fires. Replaced with a
    signature over the routines the payload calls.
  * FREE POSITION reads analog stick bytes that Tomba 2's digital-pinned pad
    never receives. Unresolved -- see the warning above.

Neither adaptation changes the menu; both exist so it can run here at all.
