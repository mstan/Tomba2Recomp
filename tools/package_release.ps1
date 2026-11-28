# Compatibility entry point. Regional identity and payloads live in release TOML.
param(
    [string]$Version = "",
    [ValidateSet('usa', 'ita')][string]$Variant = 'usa',
    [string]$BuildDir = 'build-release',
    [ValidateSet('ENHANCED')][string]$ExecutionProfile = 'ENHANCED',
    [string]$RecompilerBuildDir = 'recompiler/build-t2',
    [switch]$SkipRegen,
    [ValidateRange(0,64)][int]$Jobs = 2,
    [switch]$NormalPriority,
    [ValidateSet('public','playtest')][string]$Channel = 'public',
    [string]$ToolchainBin = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$versionFile = Join-Path $root 'packaging/release/VERSION'
if ($Version -and $Version.TrimStart('v') -ne (Get-Content -LiteralPath $versionFile -Raw).Trim().TrimStart('v')) {
    throw 'Update packaging/release/VERSION before packaging a different version.'
}
if (-not $NormalPriority) {
    [System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
}
$config = if ($Variant -eq 'ita') { 'packaging/release-ita.toml' } else { 'packaging/release.toml' }
$parameters = @('--root', $root, '--config', $config, '--channel', $Channel,
    '--platform', 'windows', '--jobs', [string][Math]::Max(1,$Jobs),
    '--work-dir', (Join-Path $root $BuildDir),
    '--recompiler-build-dir', (Join-Path $root $RecompilerBuildDir))
if ($SkipRegen) { $parameters += '--sources-prepared' }
if ($ToolchainBin) { $parameters += @('--toolchain-bin', $ToolchainBin) }
& (Join-Path $root 'psxrecomp-v4/tools/package_title_release.ps1') @parameters
exit $LASTEXITCODE
