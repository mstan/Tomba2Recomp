param(
    [string]$Version = "v0.0.2",
    [string]$BuildDir = "build-release",
    # Where the accumulated overlay cache lives (compile_overlays.py --out-dir,
    # per game.toml overlay_autocompile_cmd). Bundled as a head start; optional.
    [string]$CacheBuildDir = "build-t2",
    # Ship the checked-in generated/ code as-is instead of regenerating.
    # Use when the runtime changed but codegen did not: regenerating with a
    # newer emitter would swap in code the release validation never ran
    # (decoder/emitter changes require a fresh user playthrough).
    [switch]$SkipRegen
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildPath = Join-Path $Root $BuildDir
$StageRoot = Join-Path $Root "release-stage"
$Stage = Join-Path $StageRoot "Tomba2Recomp-windows-x64"
$ZipPath = Join-Path $Root ("Tomba2Recomp-{0}-windows-x64.zip" -f $Version)
$MingwBin = "C:\msys64\mingw64\bin"

$env:PATH = "$MingwBin;$env:PATH"

# cmake writes benign warnings to STDERR; under Stop, PS 5.1 promotes native
# stderr to a terminating error. Gate on $LASTEXITCODE instead (house pattern).
function Invoke-Native {
    param([scriptblock]$Cmd, [string]$What)
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Cmd 2>&1 | Out-Host
    $code = $LASTEXITCODE
    $ErrorActionPreference = $old
    if ($code -ne 0) { throw "$What failed (exit $code)" }
}

# Framework via THIS repo's junction (psxrecomp-v4), so the release always
# builds against the pinned framework tree, never a sibling checkout.
$RecompDir = Resolve-Path (Join-Path $Root "psxrecomp-v4\recompiler\build-t2")
Invoke-Native { cmake --build $RecompDir --target psxrecomp-game -j $env:NUMBER_OF_PROCESSORS } "recompiler build"
if ($SkipRegen) {
    Write-Host "SkipRegen: shipping checked-in generated/ code (validated bits) without regeneration"
} else {
    & (Join-Path $RecompDir "psxrecomp-game.exe") --config (Join-Path $Root "game.toml")
    if ($LASTEXITCODE -ne 0) { throw "game regen failed" }
}

Invoke-Native { cmake -S $Root -B $BuildPath -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSX_DEBUG_TOOLS=OFF -DPSX_LAUNCHER=ON } "cmake configure"
Invoke-Native { cmake --build $BuildPath -j $env:NUMBER_OF_PROCESSORS } "cmake build"

if (Test-Path $StageRoot) {
    Remove-Item -Recurse -Force $StageRoot
}
New-Item -ItemType Directory -Force $Stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Stage "saves") | Out-Null

$DevExe = Join-Path $BuildPath "Tomba2Recomp.exe"
if (-not (Test-Path $DevExe)) { $DevExe = Join-Path $BuildPath "psx-runtime.exe" }
Copy-Item $DevExe (Join-Path $Stage "Tomba2Recomp.exe")
Copy-Item (Join-Path $Root "README.md") $Stage
Copy-Item (Join-Path $Root "LICENSE") $Stage
if (Test-Path (Join-Path $Root "RELEASE_NOTES.md")) {
    Copy-Item (Join-Path $Root "RELEASE_NOTES.md") $Stage
}

# Launcher assets (RML + fonts + images), staged next to the exe by the build.
$LauncherRml = Join-Path $BuildPath "launcher.rml"
if (-not (Test-Path $LauncherRml)) {
    throw "Launcher assets missing at $BuildPath (no launcher.rml) -- was the build configured with -DPSX_LAUNCHER=ON?"
}
Copy-Item $LauncherRml $Stage
foreach ($dir in @("fonts","img")) {
    $src = Join-Path $BuildPath $dir
    if (-not (Test-Path $src)) { throw "Launcher asset dir missing: $src" }
    Copy-Item -Recurse -Force $src (Join-Path $Stage $dir)
}
$fontCount = (Get-ChildItem (Join-Path $Stage "fonts") -Filter *.ttf -ErrorAction SilentlyContinue).Count
$imgCount  = (Get-ChildItem (Join-Path $Stage "img") -Filter *.png -ErrorAction SilentlyContinue).Count
Write-Host "Bundled launcher assets: launcher.rml + $fontCount font(s) + $imgCount image(s)"

# Player-facing game.toml: same effective runtime settings as the dev config,
# minus dev-only sections (debug port, overlay autocompile command, [audit]).
@"
[game]
name = "Tomba! 2 - The Evil Swine Return"
id = "SCUS-94454"
exe = "tomba2/SCUS_944.54"
disc = "tomba2/Tomba! 2 - The Evil Swine Return (USA).cue"
load_address = "0x80010000"
entry_pc = "0x80018B6C"
text_size = "0x00028800"
stack_base = "0x801FFFF0"

# Required block; used only by the developer recompiler tool, not at runtime.
[recompiler]
seeds = "seeds/ghidra_funcs.txt"
out_dir = "generated"

# ---- Player-adjustable options ------------------------------------------
# Edit, save, and restart Tomba2Recomp.exe to apply.
[runtime]
window_title = "Tomba! 2 Recompiled"
memcard_dir = "saves"

# Skip the PlayStation boot animation (the game's own kernel setup and disc
# load still run for real, fast-forwarded). Set false for the fully faithful
# boot, logos included.
fast_boot = true

# Overlay cache: keeps converted native code for game areas in the cache
# folder, and records newly visited areas into overlay_captures.json so your
# own cache grows as you play. Keep that file private - it contains game code
# from your disc (see README).
overlay_cache = true
# Small timing-sensitive splash/FMV setup routines stay on the interpreter.
overlay_native_block = [
  "0x80096A90",
  "0x80052078",
  "0x800520C0",
]

# ---- Visual quality -----------------------------------------------------
[video]
# renderer: "software" (CPU renderer, this release's default) or "opengl"
# (hardware GPU renderer). Software is the default while an OpenGL
# performance issue specific to Tomba! 2 is being fixed (FMVs and gameplay
# run visibly slower on the GL path).
renderer          = "software"
# supersampling: render at this multiple of native resolution. 1 = native PSX.
supersampling     = 1
texture_filtering = "nearest"
# Widescreen ships OFF (authentic 4:3). Opt in from the launcher's
# experimental Widescreen toggle (16:9 / 21:9 native-wide gameplay).
aspect_ratio      = "4:3"
# FMV auto-skip stays off for the faithful presentation; the launcher's FMV
# toggle enables it. These two settings make that toggle also cover the
# silent RAM-preloaded Whoopee Camp logo (it streams no XA audio, so the
# default movie detector alone would miss it).
fmv_skip_no_xa      = true
fmv_skip_no_xa_hold = 600

# ---- Widescreen (experimental) -------------------------------------------
# 16:9 / 21:9 native-wide gameplay, launcher/settings opt-in; the 4:3 default
# is byte-identical to the original presentation. Gameplay frames are
# detected by GTE activity; menus/FMV stay authored 4:3.
[widescreen]
gte_game_mode    = true
nw_flat_backdrop = true
nw_phase_backdrop = true

[widescreen.cull]
# Widen the game's own screen-extent culls only while a wide aspect is
# active (identity at 4:3).
auto_screen_x = true
screen_w_imms = ["0x140"]
screen_h_imms = ["0xF0"]

# ---- Controller ---------------------------------------------------------
# Tomba! 2 is a d-pad platformer: real hardware boots a DualShock in DIGITAL
# mode and the game expects it. Analog modes are not offered in this release
# (the DualShock config-mode handshake is not fully emulated yet).
[controller]
default_mode = "digital"
allow_hybrid = false
lock_mode    = true
"@ | Set-Content -Encoding ASCII (Join-Path $Stage "game.toml")

# Prebuilt overlay cache: only .dll + .ranges, only THIS build's codegen tag.
$RecompTools = Resolve-Path (Join-Path $Root "psxrecomp-v4\tools")
$RecompInc   = Resolve-Path (Join-Path $Root "psxrecomp-v4\runtime\include")
$tagScript = Join-Path $env:TEMP ("psx_cgtag_{0}.py" -f $PID)
@"
import importlib.util
s = importlib.util.spec_from_file_location('co', r'$RecompTools\compile_overlays.py')
m = importlib.util.module_from_spec(s); s.loader.exec_module(m)
inc = r'$RecompInc'
print('cg%d_%08x' % (m.codegen_ver(inc), m.codegen_hash(inc)))
"@ | Set-Content -Encoding ASCII $tagScript
$CgTag = (& python $tagScript).Trim()
Remove-Item -Force $tagScript
Write-Host "Release codegen tag: $CgTag (only this cache namespace is shipped)"
$CacheSrc = Join-Path $Root "$CacheBuildDir/cache/SCUS-94454"
if (Test-Path $CacheSrc) {
    $CacheDst = Join-Path $Stage "cache/SCUS-94454"
    $cacheFiles = @(Get-ChildItem $CacheSrc -Recurse -File -Include *.dll,*.ranges |
        Where-Object { $_.FullName -notmatch '[\\/]sljit[\\/]' -and $_.FullName -match "[\\/]$CgTag[\\/]" })
    if ($cacheFiles.Count -eq 0) {
        # A tag mismatch means the accumulated cache predates this build's
        # runtime includes. Shipping a foreign-tag cache would be dead weight
        # (the runtime only loads its own namespace), so fail loudly: rebuild
        # the cache under this tag (compile_overlays.py) or pass a different
        # -CacheBuildDir.
        throw ("Overlay cache at $CacheSrc has no shards for this build's codegen tag $CgTag - " +
               "rebuild the cache with compile_overlays.py against this runtime, or release without a cache deliberately")
    }
    foreach ($f in $cacheFiles) {
        $rel  = $f.FullName.Substring($CacheSrc.Length).TrimStart('\','/')
        $dest = Join-Path $CacheDst $rel
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        Copy-Item $f.FullName $dest
    }
    $dllCount = (Get-ChildItem $CacheDst -Recurse -Filter *.dll).Count
    Write-Host "Bundled overlay cache: $dllCount native overlay DLL(s)"
} else {
    Write-Warning "No overlay cache found at $CacheSrc - releasing without bundled cache"
}

# ---- Self-contained overlay toolchain (tcc tier) -------------------------
$Toolchain = Join-Path $Stage "overlay_toolchain"
New-Item -ItemType Directory -Force $Toolchain | Out-Null
$DlCache = Join-Path $Root "tools/_toolchain_cache"
New-Item -ItemType Directory -Force $DlCache | Out-Null

$PyVer = "3.13.1"
$PyZip = Join-Path $DlCache "python-$PyVer-embed-amd64.zip"
if (-not (Test-Path $PyZip)) {
    Invoke-WebRequest -Uri "https://www.python.org/ftp/python/$PyVer/python-$PyVer-embed-amd64.zip" -OutFile $PyZip
}
Expand-Archive -Path $PyZip -DestinationPath (Join-Path $Toolchain "python") -Force

$TccZip = Join-Path $DlCache "tcc-0.9.27-win64-bin.zip"
if (-not (Test-Path $TccZip)) {
    Invoke-WebRequest -Uri "https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip" -OutFile $TccZip
}
$TccTmp = Join-Path $DlCache "tcc_extract"
if (Test-Path $TccTmp) { Remove-Item -Recurse -Force $TccTmp }
Expand-Archive -Path $TccZip -DestinationPath $TccTmp -Force
Copy-Item -Recurse -Force (Join-Path $TccTmp "tcc") (Join-Path $Toolchain "tcc")

Copy-Item (Join-Path $RecompDir "psxrecomp-game.exe") $Toolchain
foreach ($d in @("libgcc_s_seh-1.dll","libstdc++-6.dll","libwinpthread-1.dll")) {
    Copy-Item (Join-Path $MingwBin $d) $Toolchain
}
Copy-Item (Join-Path $RecompTools "compile_overlays.py") $Toolchain
$ToolInc = Join-Path $Toolchain "include"
New-Item -ItemType Directory -Force $ToolInc | Out-Null
Copy-Item (Join-Path $RecompInc "*.h") $ToolInc
$tcMB = "{0:N0}" -f ((Get-ChildItem $Toolchain -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)
Write-Host "Bundled overlay toolchain (embedded python + tcc + recompiler): ~$tcMB MB"

# Assert self-containment (imports only Windows system DLLs).
$objdump = Join-Path $MingwBin "objdump.exe"
$imports = & $objdump -p (Join-Path $Stage "Tomba2Recomp.exe") |
    Select-String "DLL Name: (.+)" | ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
$systemDlls = @("kernel32.dll","user32.dll","gdi32.dll","shell32.dll","msvcrt.dll",
                "advapi32.dll","ws2_32.dll","comdlg32.dll","dbghelp.dll","ole32.dll",
                "oleaut32.dll","winmm.dll","imm32.dll","version.dll","setupapi.dll",
                "dinput8.dll","rpcrt4.dll","hid.dll","cfgmgr32.dll","opengl32.dll")
$nonSystem = $imports | Where-Object { $systemDlls -notcontains $_.ToLower() }
if ($nonSystem) {
    throw "Release exe is NOT self-contained -- imports non-system DLL(s): $($nonSystem -join ', ')"
}
Write-Host "Verified self-contained: imports only system DLLs ($($imports.Count) total)"

@"
; PSXRecomp input mapping. PSX buttons are active when any listed source is pressed.
; Sources use SDL/Xbox names: a,b,x,y,back,start,leftshoulder,rightshoulder,
; lefttrigger,righttrigger,dpup,dpdown,dpleft,dpright,leftx-/leftx+/lefty-/lefty+.

[controller]
enabled = true
device = 0
deadzone = 12000

[mapping]
up = dpup,lefty-
down = dpdown,lefty+
left = dpleft,leftx-
right = dpright,leftx+
cross = a
circle = b
square = x
triangle = y
l1 = leftshoulder
r1 = rightshoulder
l2 = lefttrigger
r2 = righttrigger
start = start
select = back
"@ | Set-Content -Encoding ASCII (Join-Path $Stage "input.ini")

@"
Tomba2Recomp $Version

Tomba! 2: The Evil Swine Return boots from the PlayStation BIOS and plays -
through the intro, the title screen, the attract demos, and into gameplay,
with working controller input and no known crashes. It has not been verified
through a full playthrough yet, so treat it as a very playable preview.

New in this release:
- CRITICAL boot fix: earlier zips could fail to start on user machines
  (a reference copy of the game's boot executable only exists in developer
  checkouts; the runtime now reads it directly from your disc image).
- Experimental widescreen: 16:9 / 21:9 native-wide gameplay, opt-in from the
  launcher's Widescreen toggle. The 4:3 default is byte-identical to the
  original presentation; menus and FMVs stay authored 4:3.
- The software renderer is now the default: the OpenGL path currently runs
  Tomba! 2 visibly slower (FMVs and gameplay) and stays selectable in the
  launcher until that issue is fixed.
- Memory card saving and loading (from v0.0.2) carry forward.

This package does not include the Tomba! 2 disc, the PlayStation BIOS, save
data, or any game assets - you supply those from your own collection, and
Tomba2Recomp asks for them one at a time (each dialog says which one it
wants). The executable and the cache folder contain statically recompiled
(machine-translated) builds of the game's code.

Known items in this release:
- OpenGL renderer is slower than software for this title (see above).
- Analog controller modes are not offered (the game is digital-native).
"@ | Set-Content -Encoding ASCII (Join-Path $Stage "RELEASE.txt")

if (Test-Path $ZipPath) { Remove-Item -Force $ZipPath }
Compress-Archive -Path $Stage -DestinationPath $ZipPath
$zipMB = "{0:N1}" -f ((Get-Item $ZipPath).Length / 1MB)
Write-Host "Release packaged: $ZipPath (~$zipMB MB)"
