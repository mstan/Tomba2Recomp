param(
    # Empty means "read packaging/release/VERSION", the single source of truth
    # shared with tools/package_appimage.sh so the two platforms cannot ship
    # different version strings.
    [string]$Version = "",
    [ValidateSet("usa", "ita")]
    [string]$Variant = "usa",
    [string]$BuildDir = "build-release",
    # Where the accumulated overlay cache lives (compile_overlays.py --out-dir,
    # per game.toml overlay_autocompile_cmd). Bundled as a head start; optional.
    [string]$CacheBuildDir = "build-t2",
    # Ship the checked-in generated/ code as-is instead of regenerating.
    # Use when the runtime changed but codegen did not: regenerating with a
    # newer emitter would swap in code the release validation never ran
    # (decoder/emitter changes require a fresh user playthrough).
    [switch]$SkipRegen,
    # Parallel compile jobs. 0 = leave two cores for the rest of the machine.
    # Packaging a release must not make the box unusable.
    [int]$Jobs = 0,
    # Packaging runs below normal priority BY DEFAULT (every cmake/ninja/gcc
    # child inherits it). Pass -NormalPriority when you want the machine's
    # full attention.
    [switch]$NormalPriority,
    # Ship without a bundled overlay cache. Off by default: a cache-less
    # package makes every player's first session run overlays interpreted, so
    # it has to be asked for rather than warned about.
    [switch]$AllowNoCache
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$PackagingRelease = Join-Path $Root "packaging\release"
if (-not $Version) {
    $VersionFile = Join-Path $PackagingRelease "VERSION"
    if (-not (Test-Path -LiteralPath $VersionFile)) {
        throw "No -Version given and $VersionFile is missing"
    }
    $Version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
    if (-not $Version) { throw "$VersionFile is empty" }
}
$BuildPath = Join-Path $Root $BuildDir
$StageRoot = Join-Path $Root "release-stage"
$RuntimeTarget = "psx-runtime"
$ExeStem = "Tomba2Recomp"
$StageName = "Tomba2Recomp-windows-x64"
$GameConfigName = "game.toml"
$GameConfigSource = Join-Path $PackagingRelease $GameConfigName
$RegenConfig = Join-Path $Root "game.toml"
$CacheGameId = "SCUS-94454"
$ExpectedMods = 8
$ReleaseTitle = "Tomba! 2 Recompiled"
if ($Variant -eq "ita") {
    $RuntimeTarget = "psx-runtime-ita"
    $ExeStem = "Tombi2Recomp-ita"
    $StageName = "Tombi2Recomp-ita-windows-x64"
    $GameConfigName = "game_ita.toml"
    $GameConfigSource = Join-Path $PackagingRelease $GameConfigName
    $RegenConfig = Join-Path $Root "game_ita.toml"
    $CacheGameId = "SCES-02686"
    $ExpectedMods = 7
    $ReleaseTitle = "Tombi! 2 (Italian) Recompiled"
}
$Stage = Join-Path $StageRoot $StageName
$ZipPath = Join-Path $Root ("{0}-{1}-windows-x64.zip" -f $ExeStem, $Version)
$MingwBin = "C:\msys64\mingw64\bin"

$env:PATH = "$MingwBin;$env:PATH"

if ($Jobs -le 0) {
    $Jobs = [Math]::Max(2, [int]$env:NUMBER_OF_PROCESSORS - 2)
}
if (-not $NormalPriority) {
    # Child processes inherit the priority class, so setting it once here
    # covers cmake, ninja, every gcc, and the MSYS bash used for regen_bios.
    [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass =
        [System.Diagnostics.ProcessPriorityClass]::BelowNormal
}
Write-Host ("Packaging {0} {1} ({2}, jobs={3}, priority={4})" -f `
    $ExeStem, $Version, $Variant, $Jobs, [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass)

# ---- Path helpers ---------------------------------------------------------
# PowerShell's Copy-Item decides "is the destination a file or a directory?"
# from whether the destination EXISTS. That makes two silent failure modes:
#
#   Copy-Item file.txt C:\stage\sub\        -> if sub\ does not exist yet,
#                                              a FILE named "sub" is created
#   Copy-Item -Recurse dir C:\stage\assets  -> if assets\ already exists, the
#                                              tree nests as assets\assets
#
# Both produce a package that looks built but is wrong. These helpers make the
# intent explicit instead of inferred, and use -LiteralPath throughout so game
# paths containing [ ] or other wildcard metacharacters are never globbed.
function New-Dir {
    param([Parameter(Mandatory)][string]$Path)
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        throw "Expected a directory but a file exists at: $Path"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        # Windows PowerShell 5.1's New-Item has no -LiteralPath, so its -Path
        # still glob-expands. Create through .NET instead: literal by
        # definition, and it makes intermediate directories.
        [System.IO.Directory]::CreateDirectory($Path) | Out-Null
    }
    return $Path
}
function Copy-FileTo {
    # Copy a single file to an explicit destination FILE path.
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Copy-FileTo: source file not found: $Source"
    }
    if (Test-Path -LiteralPath $Destination -PathType Container) {
        throw "Copy-FileTo: destination is an existing directory: $Destination"
    }
    New-Dir (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}
function Copy-FileInto {
    # Copy a single file INTO an explicit destination DIRECTORY, keeping its name.
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$DestinationDir
    )
    New-Dir $DestinationDir | Out-Null
    Copy-FileTo -Source $Source -Destination (Join-Path $DestinationDir (Split-Path -Leaf $Source))
}
function Copy-TreeTo {
    # Replace the destination directory with a copy of the source tree. Never
    # nests, never merges into a stale tree.
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination
    )
    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Copy-TreeTo: source directory not found: $Source"
    }
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    New-Dir (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

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

# ---- BIOS backends --------------------------------------------------------
# The runtime refuses to configure without at least one recompiled BIOS in
# psxrecomp-v4/generated (the require-generated guard). A clean checkout has
# none, so a "clone and run the packager" path fails at cmake with a message
# about a script the packager could simply have run. Generate the bundled
# OpenBIOS (MIT, no dump needed) and, when a retail dump is present, SCPH1001.
function Ensure-BiosBackends {
    param([Parameter(Mandatory)][string]$FrameworkRoot)
    $stems = @()
    if (Test-Path -LiteralPath (Join-Path $FrameworkRoot "bios\OpenBIOS.toml")) {
        $stems += ,@("OpenBIOS", "bios/OpenBIOS.toml")
    }
    if (Test-Path -LiteralPath (Join-Path $FrameworkRoot "bios\SCPH1001.BIN")) {
        $stems += ,@("SCPH1001", "bios/SCPH1001.toml")
    }
    if (-not $stems) { throw "No BIOS profile available under $FrameworkRoot\bios" }

    $missing = @($stems | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $FrameworkRoot ("generated\{0}_dispatch.c" -f $_[0])))
    })
    if (-not $missing) { return }

    $bash = $null
    foreach ($cand in @("C:\msys64\usr\bin\bash.exe", "C:\msys64\mingw64\bin\bash.exe")) {
        if (Test-Path -LiteralPath $cand) { $bash = $cand; break }
    }
    if (-not $bash) {
        throw ("Missing recompiled BIOS backend(s): {0}. Install MSYS2 or run " +
               "psxrecomp-v4/tools/regen_bios.sh manually." -f (($missing | ForEach-Object { $_[0] }) -join ', '))
    }
    # MSYS bash needs a POSIX path; cygpath is the supported converter and
    # handles drive letters and spaces that a naive string replace would not.
    $cygpath = Join-Path (Split-Path -Parent $bash) "cygpath.exe"
    $posixRoot = (& $cygpath -u $FrameworkRoot).Trim()
    # A login shell (-l) rebuilds PATH from /etc/profile, which drops the
    # MinGW toolchain, so regen_bios.sh cannot find cmake. Use a non-login
    # shell and prepend the same compiler directory this script already uses,
    # converted with cygpath rather than by rewriting the drive letter.
    $posixMingw = (& $cygpath -u $MingwBin).Trim()
    foreach ($stem in $missing) {
        Write-Host "Generating recompiled BIOS backend: $($stem[0])"
        # Name this anything but $cmd: PowerShell variables are case-insensitive,
        # so a $cmd here binds to Invoke-Native's own [scriptblock]$Cmd parameter
        # when the scriptblock runs, and bash receives the scriptblock's source
        # text instead of the command.
        $biosShellCmd = "export PATH='$posixMingw':`$PATH; cd '$posixRoot' && " +
                        "PSXRECOMP_BIOS_BUILD=recompiler/build tools/regen_bios.sh --config $($stem[1])"
        Invoke-Native { & $bash -c $biosShellCmd } "regen_bios ($($stem[0]))"
    }
}

# Framework via THIS repo's junction (psxrecomp-v4), so the release always
# builds against the pinned framework tree, never a sibling checkout.
$RecompDir = Resolve-Path (Join-Path $Root "psxrecomp-v4\recompiler\build-t2")
Invoke-Native { cmake --build $RecompDir --target psxrecomp-game -j $Jobs } "recompiler build"
Ensure-BiosBackends -FrameworkRoot (Join-Path $Root "psxrecomp-v4")
if ($SkipRegen) {
    Write-Host "SkipRegen: shipping checked-in generated/ code (validated bits) without regeneration"
} else {
    & (Join-Path $RecompDir "psxrecomp-game.exe") --config $RegenConfig
    if ($LASTEXITCODE -ne 0) { throw "game regen failed" }
}

# --no-insert-timestamp: the MinGW linker stamps the current time into the PE
# header, which is the only thing that stopped two runs of this packager from
# producing a byte-identical exe (measured: exactly 2 differing bytes in an
# 18 MB binary). Release builds want the artifact to be a function of its
# sources, not of the clock.
Invoke-Native {
    cmake -S $Root -B $BuildPath -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DPSX_DEBUG_TOOLS=OFF `
        "-DCMAKE_EXE_LINKER_FLAGS=-Wl,--no-insert-timestamp"
} "cmake configure"
Invoke-Native { cmake --build $BuildPath --target $RuntimeTarget -j $Jobs } "cmake build"

if (Test-Path -LiteralPath $StageRoot) {
    Remove-Item -LiteralPath $StageRoot -Recurse -Force
}
New-Dir $Stage | Out-Null
New-Dir (Join-Path $Stage "saves") | Out-Null

$DevExe = Join-Path $BuildPath "$ExeStem.exe"
if (-not (Test-Path -LiteralPath $DevExe)) { $DevExe = Join-Path $BuildPath "$RuntimeTarget.exe" }
Copy-FileTo $DevExe (Join-Path $Stage "$ExeStem.exe")
Copy-FileInto (Join-Path $Root "README.md") $Stage
Copy-FileInto (Join-Path $Root "LICENSE") $Stage
Copy-FileInto (Join-Path $PackagingRelease "START_HERE.txt") $Stage
$BundledBiosSrc = Join-Path $BuildPath "bios"
if (!(Test-Path (Join-Path $BundledBiosSrc "openbios.bin")) -or
    (Get-Item (Join-Path $BundledBiosSrc "openbios.bin")).Length -ne 524288 -or
    !(Test-Path (Join-Path $BundledBiosSrc "OpenBIOS.LICENSE"))) {
    throw "Runtime build did not stage OpenBIOS and its MIT notice"
}
$BundledBiosDst = New-Dir (Join-Path $Stage "bios")
Copy-FileInto (Join-Path $BundledBiosSrc "openbios.bin") $BundledBiosDst
Copy-FileInto (Join-Path $BundledBiosSrc "OpenBIOS.LICENSE") $BundledBiosDst
if (Test-Path -LiteralPath (Join-Path $Root "RELEASE_NOTES.md")) {
    Copy-FileInto (Join-Path $Root "RELEASE_NOTES.md") $Stage
}

# Launcher assets: this build ships the shared recomp-ui Dear ImGui launcher
# (RECOMP_LAUNCHER; see main.cpp + recomp-ui/recomp_ui.cmake), which loads from
# <exe>/assets/ (fonts + img TGAs, including this repo's boxart baked in by
# recomp_target_launcher_ui's POST_BUILD).
$AssetsSrc = Join-Path $BuildPath "assets"
if (-not (Test-Path (Join-Path $AssetsSrc "img"))) {
    throw "recomp-ui launcher assets missing at $AssetsSrc -- was the recomp-ui launcher built (recomp-ui junction present)?"
}
Copy-TreeTo $AssetsSrc (Join-Path $Stage "assets")
$fontCount = (Get-ChildItem (Join-Path $Stage "assets/fonts") -Filter *.ttf -ErrorAction SilentlyContinue).Count
$imgCount  = (Get-ChildItem (Join-Path $Stage "assets/img")   -Filter *.tga -ErrorAction SilentlyContinue).Count
Write-Host "Bundled recomp-ui launcher assets: $fontCount font(s) + $imgCount image(s)"

# Game-owned display enhancements are staged by CMake beside the development
# executable. Preserve that exact catalog in the release package.
$ModsSrc = Join-Path $BuildPath "mods"
if (-not (Test-Path (Join-Path $ModsSrc "packages"))) {
    throw "Tomba 2 preloaded mod catalog missing at $ModsSrc"
}
Copy-TreeTo $ModsSrc (Join-Path $Stage "mods")
$modManifestCount = (Get-ChildItem (Join-Path $Stage "mods/packages") -Recurse -Filter manifest.toml).Count
if ($modManifestCount -ne $ExpectedMods) {
    throw "Expected $ExpectedMods mod manifests for $Variant, found $modManifestCount"
}
Write-Host "Bundled Tomba 2 mod catalog: $modManifestCount package(s)"

# Player-facing game.toml: same effective runtime settings as the dev config,
# minus dev-only sections (debug port, overlay autocompile command, [audit]).
# Player-facing game.toml comes from packaging/release/game.toml, the same
# file tools/package_appimage.sh ships, so Windows and Linux cannot drift.
Copy-FileTo $GameConfigSource (Join-Path $Stage $GameConfigName)

# Prebuilt overlay cache: DLLs, range manifests, and exact-hash BIOS-resident
# sidecars; only THIS build's codegen tag.
$RecompTools = Resolve-Path (Join-Path $Root "psxrecomp-v4\tools")
$RecompInc   = Resolve-Path (Join-Path $Root "psxrecomp-v4\runtime\include")
$tagScript = Join-Path $env:TEMP ("psx_cgtag_{0}.py" -f $PID)
@"
import importlib.util
s = importlib.util.spec_from_file_location('co', r'$RecompTools\compile_overlays.py')
m = importlib.util.module_from_spec(s); s.loader.exec_module(m)
inc = r'$RecompInc'
print('cg%d_%08x_gc%08x' % (
        m.codegen_ver(inc),
        m.codegen_hash(inc),
        m.overlay_config_hash(
            r'$(Join-Path $RecompDir "psxrecomp-game.exe")',
            r'$(Join-Path $Stage $GameConfigName)')))
"@ | Set-Content -Encoding ASCII $tagScript
$CgTag = (& py -3 $tagScript).Trim()
Remove-Item -Force $tagScript
Write-Host "Release codegen tag: $CgTag (only this cache namespace is shipped)"
$CacheSrc = Join-Path $Root "$CacheBuildDir/cache/$CacheGameId"
if (Test-Path $CacheSrc) {
    $CacheDst = Join-Path $Stage "cache/$CacheGameId"
    $cacheFiles = @(Get-ChildItem $CacheSrc -Recurse -File -Include *.dll,*.ranges,*.resident |
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
} elseif ($AllowNoCache) {
    Write-Warning "No overlay cache at $CacheSrc - shipping without one because -AllowNoCache was given"
} else {
    # Shipping without a cache is a real downgrade for players: every overlay
    # runs interpreted until their own cache fills. That must be a deliberate
    # choice, not something a warning scrolls past during a release.
    throw @"
No overlay cache found at $CacheSrc, so this package would ship without one
and every player's first session would run overlays interpreted.

Build a cache for THIS release's codegen tag ($CgTag) - note the tag is
derived from the packaged player game.toml, so a cache built against the dev
game.toml lands under a different tag and will not be picked up:

  `$env:PSX_OVERLAY_CACHE_DIR = "$Root\$CacheBuildDir\cache"
  `$env:PSX_OVERLAY_CAPTURES  = "<coverage vault>\overlay_captures.json"
  python psxrecomp-v4\tools\compile_overlays.py ``
      --game-toml packaging\release\game.toml ``
      --recompiler psxrecomp-v4\recompiler\build-t2\psxrecomp-game.exe ``
      --runtime-include psxrecomp-v4\runtime\include ``
      --gcc C:\msys64\mingw64\bin\gcc.exe

Then re-run this packager. Pass -AllowNoCache to ship without one anyway.
"@
}

# ---- Self-contained overlay toolchain (tcc tier) -------------------------
# The two archives below are fetched from the network. Pin them by SHA256 and
# verify every time (including cache hits): an unverified download makes the
# package non-reproducible and trusts whatever the mirror served that day.
# Transient mirror failures (python.org and savannah both 502 periodically)
# are retried rather than losing an entire release build.
$Toolchain = New-Dir (Join-Path $Stage "overlay_toolchain")
$DlCache = New-Dir (Join-Path $Root "tools\_toolchain_cache")

function Get-PinnedArchive {
    param(
        [Parameter(Mandatory)][string]$Uri,
        [Parameter(Mandatory)][string]$Sha256,
        [Parameter(Mandatory)][string]$Destination,
        [int]$Retries = 4
    )
    $name = Split-Path -Leaf $Destination
    if (Test-Path -LiteralPath $Destination) {
        $have = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLower()
        if ($have -eq $Sha256.ToLower()) { return $Destination }
        Write-Warning "$name in the download cache has SHA256 $have (expected $Sha256); refetching"
        Remove-Item -LiteralPath $Destination -Force
    }
    for ($attempt = 1; $attempt -le $Retries; $attempt++) {
        try {
            $tmp = "$Destination.tmp"
            if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }
            Invoke-WebRequest -Uri $Uri -OutFile $tmp -UseBasicParsing
            $got = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash.ToLower()
            if ($got -ne $Sha256.ToLower()) {
                Remove-Item -LiteralPath $tmp -Force
                throw "SHA256 mismatch for $name : got $got, expected $Sha256"
            }
            Move-Item -LiteralPath $tmp -Destination $Destination -Force
            return $Destination
        } catch {
            if ($attempt -eq $Retries) {
                throw ("Failed to fetch $name after $Retries attempts: $($_.Exception.Message). " +
                       "Place a verified copy at $Destination and re-run.")
            }
            $delay = [Math]::Min(30, [Math]::Pow(2, $attempt))
            Write-Warning "$name fetch attempt $attempt failed ($($_.Exception.Message)); retrying in ${delay}s"
            Start-Sleep -Seconds $delay
        }
    }
}

$PyVer = "3.13.1"
$PyZip = Get-PinnedArchive `
    -Uri "https://www.python.org/ftp/python/$PyVer/python-$PyVer-embed-amd64.zip" `
    -Sha256 "7b7923ff0183a8b8fca90f6047184b419b108cb437f75fc1c002f9d2f8bcec16" `
    -Destination (Join-Path $DlCache "python-$PyVer-embed-amd64.zip")
Expand-Archive -LiteralPath $PyZip -DestinationPath (Join-Path $Toolchain "python") -Force

$TccZip = Get-PinnedArchive `
    -Uri "https://download.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip" `
    -Sha256 "34a721949a2583fdff725312da092fa0f5f1f284b702e6f811c6954714faabb2" `
    -Destination (Join-Path $DlCache "tcc-0.9.27-win64-bin.zip")
$TccTmp = Join-Path $DlCache "tcc_extract"
if (Test-Path -LiteralPath $TccTmp) { Remove-Item -LiteralPath $TccTmp -Recurse -Force }
Expand-Archive -LiteralPath $TccZip -DestinationPath $TccTmp -Force
Copy-TreeTo (Join-Path $TccTmp "tcc") (Join-Path $Toolchain "tcc")

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
$imports = & $objdump -p (Join-Path $Stage "$ExeStem.exe") |
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

# No baked build-machine paths: an absolute BIOS default baked into the exe
# makes it silently load the BUILDER'S BIOS wherever that path exists, so the
# clean-install picker flow is never exercised where releases are validated
# (this shipped in v0.0.4 and masked GH issue #1's setup on the dev machine).
$exeBytes = [System.IO.File]::ReadAllBytes((Join-Path $Stage "$ExeStem.exe"))
$exeText  = [System.Text.Encoding]::ASCII.GetString($exeBytes)
$bakedBios = [regex]::Matches($exeText, '[A-Za-z]:[/\\][ -~]*?SCPH1001\.BIN') | ForEach-Object { $_.Value } | Select-Object -Unique
if ($bakedBios) {
        throw "Release exe contains baked absolute BIOS path(s): $($bakedBios -join '; ') -- build with a relative DEFAULT_BIOS_PATH"
}
Write-Host "Verified no baked absolute BIOS path in the exe"

# No user-machine or copyrighted files may ride along in the stage. OpenBIOS
# and its license are intentionally bundled; retail BIOS images remain banned.
$strayPatterns = @("SCPH*.BIN","*.cue","*.iso","*.mcd","bios.cfg","disc.cfg",
                   "settings.toml","keybinds.ini","overlay_captures.json")
$stray = foreach ($pat in $strayPatterns) { Get-ChildItem $Stage -Recurse -File -Filter $pat -ErrorAction SilentlyContinue }
if ($stray) {
    throw "Stage contains files that must never ship: $(($stray | ForEach-Object FullName) -join '; ')"
}
$savesFiles = Get-ChildItem (Join-Path $Stage "saves") -Recurse -File -ErrorAction SilentlyContinue
if ($savesFiles) {
    throw "Stage saves/ directory must be empty, contains: $(($savesFiles | ForEach-Object FullName) -join '; ')"
}
Write-Host "Verified bundled OpenBIOS; no retail BIOS/disc/save/sidecar files"

# Default controller mapping: shared with the AppImage package.
Copy-FileTo (Join-Path $PackagingRelease "input.ini") (Join-Path $Stage "input.ini")

$TombaSha = (& git -C $Root rev-parse --short HEAD).Trim()
$PsxRecompSha = (& git -C (Join-Path $Root "psxrecomp-v4") rev-parse --short HEAD).Trim()

@"
$ReleaseTitle $Version

Tomba! 2: The Evil Swine Return boots from the PlayStation BIOS and plays -
through the intro, the title screen, the attract demos, and into gameplay,
with working controller input and no known crashes. It has not been verified
through a full playthrough yet, so treat it as a very playable preview.

New in this release:
- Based on Tomba2Recomp master $TombaSha and psxrecomp master $PsxRecompSha.
- Variant: $Variant.
- Tomba 2 now defaults to 2x SSAA with antialiasing enabled for the OpenGL
  renderer; lower supersampling to 1 in the launcher/settings on slower GPUs.
- Adds the conservative VSync(-1) query acceleration path used during loading,
  preserving guest timing checkpoints while bypassing side-effect-free status
  reads.
- Carries the latest psxrecomp widescreen interpreter fix, mirroring native-wide
  range sites consistently between native and interpreted execution.
- Multi-track disc support, clean first-run BIOS/disc picking, 21:9 ultrawide,
  frame interpolation, and memory card support carry forward.

This package includes the MIT-licensed OpenBIOS from PCSX-Redux and its notice
in bios/OpenBIOS.LICENSE. It does not include the Tomba! 2 disc, a retail
PlayStation BIOS, save data, or game assets.

Known items in this release:
- The software renderer remains available as a reference/fallback.
- Analog controller modes are not offered (the game is digital-native).
"@ | Set-Content -Encoding ASCII (Join-Path $Stage "RELEASE.txt")

# ---- Deterministic archive ------------------------------------------------
# Compress-Archive embeds real mtimes and walks the tree in filesystem order,
# so two identical stages produce different bytes. Build the zip by hand with
# sorted entry names and one fixed timestamp (SOURCE_DATE_EPOCH, defaulting to
# the git commit date) so a rebuild of the same sources is byte-identical and
# the published SHA256 is meaningful.
if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath -Force }

if ($env:SOURCE_DATE_EPOCH) {
    $epoch = [int64]$env:SOURCE_DATE_EPOCH
} else {
    $epoch = [int64](& git -C $Root log -1 --format=%ct).Trim()
}
$stamp = [System.DateTimeOffset]::FromUnixTimeSeconds($epoch).UtcDateTime
Write-Host ("Deterministic zip: SOURCE_DATE_EPOCH={0} ({1:yyyy-MM-dd HH:mm:ss}Z)" -f $epoch, $stamp)

Add-Type -AssemblyName System.IO.Compression | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem | Out-Null

$stageParent = Split-Path -Parent $Stage
$entries = Get-ChildItem -LiteralPath $Stage -Recurse -File |
    ForEach-Object {
        [PSCustomObject]@{
            Full = $_.FullName
            # Forward slashes, relative to the stage's parent so the archive
            # keeps its single Tomba2Recomp-windows-x64/ root folder.
            Name = $_.FullName.Substring($stageParent.Length).TrimStart('\','/').Replace('\','/')
        }
    } | Sort-Object -Property Name -CaseSensitive

$zipStream = [System.IO.File]::Open($ZipPath, [System.IO.FileMode]::CreateNew)
try {
    $archive = New-Object System.IO.Compression.ZipArchive(
        $zipStream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($e in $entries) {
            $entry = $archive.CreateEntry($e.Name, [System.IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [System.DateTimeOffset]::new($stamp, [TimeSpan]::Zero)
            $in  = [System.IO.File]::OpenRead($e.Full)
            try {
                $out = $entry.Open()
                try { $in.CopyTo($out) } finally { $out.Dispose() }
            } finally { $in.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $zipStream.Dispose() }

$zipMB = "{0:N1}" -f ((Get-Item -LiteralPath $ZipPath).Length / 1MB)
$zipSha = (Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256).Hash.ToLower()
Write-Host "Release packaged: $ZipPath (~$zipMB MB, $($entries.Count) entries)"
Write-Host "SHA256: $zipSha"
Set-Content -LiteralPath "$ZipPath.sha256" -Encoding ASCII -Value "$zipSha  $(Split-Path -Leaf $ZipPath)"
