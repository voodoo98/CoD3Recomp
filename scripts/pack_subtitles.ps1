<#
.SYNOPSIS
    Writes the translated strings back into copies of the game's .cod archives.

.DESCRIPTION
    Reads the "hu" fields of localization\strings*.json (see extract_subtitles.ps1), rebuilds the string
    bank at the end of every archive that has a translated string, and writes the changed archives to
    localization\game_hu\ with the same relative paths (the game folder itself is not touched).
    The bank may grow: the archive is extended and its header updated.

    -Install then copies them over the game folder, keeping the originals as <name>.orig
    (-Install first restores any existing .orig, so the .orig files are always the untouched English).
    -Restore puts the .orig files back (back to English) and removes hu_install.json, which lists the changed
    files with SHA-256 hashes of both versions.

    -HungarianAccents preserves the double acute letters with the matching native host font patch.
    Without it, the legacy pack remains compatible with hosts that only support o/u with umlaut.

.PARAMETER Game
    The installed game folder. Default: build\CoD3Host\game.

.PARAMETER Strings
    A strings*.json file or the folder holding them. Default: localization.

.PARAMETER Out
    Where the changed archives go. Default: localization\game_hu.

.PARAMETER Scope
    sp, mp or all (default).

.PARAMETER HungarianAccents
    Use the Hungarian font slots D5/F5/DB/FB. Requires a host with hungarian_fonts.cpp.
#>
param(
    [string]$Game,
    [string]$Strings,
    [string]$Out,
    [ValidateSet('sp', 'mp', 'all')][string]$Scope = 'all',
    [switch]$Install,
    [switch]$HungarianAccents,
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Game) { $Game = Join-Path $root 'build\CoD3Host\game' }
if (-not $Strings) { $Strings = Join-Path $root 'localization' }
if (-not $Out) { $Out = Join-Path $root 'localization\game_hu' }
if (-not (Test-Path (Join-Path $Game 'default.xex'))) { throw "no default.xex in '$Game': give the installed game folder with -Game" }
$Game = (Resolve-Path $Game).Path

$manifest = Join-Path $Game 'hu_install.json'
function Restore-Orig {
    $n = 0
    Get-ChildItem $Game -Recurse -Filter '*.cod.orig' | ForEach-Object {
        Copy-Item -Force $_.FullName ($_.FullName -replace '\.orig$', '')
        Remove-Item -Force $_.FullName
        $n++
    }
    if (Test-Path $manifest) { Remove-Item $manifest }
    return $n
}

if ($Restore) {
    "$(Restore-Orig) archives restored to the original English"
    return
}

# an earlier -Install leaves translated archives in place: go back to the originals first, so the
# string bank is always rebuilt from the English source
if ($Install) { $r = Restore-Orig; if ($r) { "$r archives restored before repacking" } }

Add-Type -Path (Join-Path $PSScriptRoot 'CoD3Loc.cs')
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
[CoD3Loc.Tool]::Pack($Game, $Strings, $Out, $Scope, [bool]$HungarianAccents)

if ($Install) {
    $outFull = (Resolve-Path $Out).Path
    $files = @()
    Get-ChildItem $outFull -Recurse -Filter '*.cod' | ForEach-Object {
        $rel = $_.FullName.Substring($outFull.Length).TrimStart('\')
        $dest = Join-Path $Game $rel
        Copy-Item -Force $dest "$dest.orig"
        Copy-Item -Force $_.FullName $dest
        $files += [pscustomobject]@{
            file        = $rel
            orig_sha256 = (Get-FileHash "$dest.orig" -Algorithm SHA256).Hash
            hu_sha256   = (Get-FileHash $dest -Algorithm SHA256).Hash
        }
        "installed $rel"
    }
    # record of what was changed, for an installer / uninstaller
    [pscustomobject]@{ language = 'hu'; font_encoding = $(if ($HungarianAccents) { 'hu-double-acute-v1' } else { 'windows-1252' }); installed = (Get-Date -Format s); files = $files } |
        ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 $manifest
    "$($files.Count) archives installed; undo with: pack_subtitles.ps1 -Restore"
}
