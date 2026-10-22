<#
.SYNOPSIS
    Pulls the English text (subtitles, menus, objectives, ...) out of the game's .cod archives.

.DESCRIPTION
    Writes localization\strings.json (or strings_001.json, strings_002.json, ... with -ChunkSize):
    one entry per distinct string, in archive order, so a scene's lines stay together.

        { "id": "14b1c781", "t": "5.8|9.9|", "en": "we had the armored unit up front.", "hu": "", "files": ["sp/saint_lo/saint_lo.cod"] }

    Translate by filling in "hu". Leave "id", "t" and "en" as they are: the packer finds the
    string again by them. "t" is the subtitle's timing (start|end|), it is put back by the packer.
    "|" inside a subtitle is a line break, "\n" a new line, "^1" and the like colour codes: keep them.
    An empty "hu" leaves the string English.

.PARAMETER Game
    The installed game folder (the one with default.xex). Default: build\CoD3Host\game.

.PARAMETER Out
    Where the JSON goes. Default: localization.

.PARAMETER Scope
    sp, mp or all (default).

.PARAMETER ChunkSize
    Entries per file; 0 (default) writes one file.
#>
param(
    [string]$Game,
    [string]$Out,
    [ValidateSet('sp', 'mp', 'all')][string]$Scope = 'all',
    [int]$ChunkSize = 0
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Game) { $Game = Join-Path $root 'build\CoD3Host\game' }
if (-not $Out) { $Out = Join-Path $root 'localization' }
if (-not (Test-Path (Join-Path $Game 'default.xex'))) { throw "no default.xex in '$Game': give the installed game folder with -Game" }
Add-Type -Path (Join-Path $PSScriptRoot 'CoD3Loc.cs')
[CoD3Loc.Tool]::Extract((Resolve-Path $Game).Path, $Out, $ChunkSize, $Scope)
