param(
    [string]$GameDir = (Join-Path $PSScriptRoot '../build/CoD3Host'),
    [string]$PayloadDir = (Join-Path $PSScriptRoot '../build/dlss5-review/runtime'),
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$manifestPath = Join-Path $GameDir 'dlss5-install.json'

function ChildPath([string]$Root, [string]$Relative) {
    $path = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (!$path.StartsWith($Root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path outside installation: $Relative"
    }
    return $path
}
function WriteUtf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function SetIni([string]$Text, [string]$Section, [string]$Key, [string]$Value) {
    $sectionPattern = '(?ms)^\[' + [regex]::Escape($Section) + '\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)'
    $match = [regex]::Match($Text, $sectionPattern)
    if (!$match.Success) { return $Text.TrimEnd() + "`r`n`r`n[$Section]`r`n$Key=$Value`r`n" }
    $body = $match.Groups['body'].Value
    $keyPattern = '(?m)^' + [regex]::Escape($Key) + '\s*=.*$'
    if ([regex]::IsMatch($body, $keyPattern)) {
        $body = [regex]::Replace($body, $keyPattern, [Text.RegularExpressions.MatchEvaluator]{ param($m) "$Key=$Value" })
    } else { $body = "$Key=$Value`r`n" + $body }
    return $Text.Remove($match.Groups['body'].Index, $match.Groups['body'].Length).Insert($match.Groups['body'].Index, $body)
}

$running = Get-Process -Name CoD3,CoD3SP_s -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -ieq $GameDir)
}
if ($running) { throw 'Close this installation of CoD3 before changing its rendering files.' }

if ($Restore) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $backup = ChildPath $GameDir $manifest.backup
    # Validate the complete manifest before restoring any file.
    foreach ($file in $manifest.files) {
        $null = ChildPath $GameDir $file.path
        if ($file.existed) {
            $original = ChildPath $backup $file.path
            if ((Get-FileHash -LiteralPath $original).Hash -ne $file.original_sha256) { throw "Backup damaged: $($file.path)" }
        }
    }
    foreach ($file in $manifest.files) {
        $destination = ChildPath $GameDir $file.path
        if ($file.existed) { Copy-Item -LiteralPath (ChildPath $backup $file.path) -Destination $destination -Force }
        elseif (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination }
    }
    Remove-Item -LiteralPath $manifestPath
    Write-Output "Original rendering files and settings restored. Backup retained: $backup"
    return
}

if (Test-Path -LiteralPath $manifestPath) { throw 'DLSS 5 is already installed; restore it before installing another payload.' }
$PayloadDir = (Resolve-Path -LiteralPath $PayloadDir).Path
$files = @('dxgi.dll', 'CoD3Depth.addon64', 'deep-fried-chicken.addon64', 'deep-fried-chicken-nvngx.dll',
    'deep-fried-chicken.cfg', 'nvngx_dlssnr.dll', 'nvngx_dlss.dll', 'dlss5-feed.addon64',
    'dlss5-feed.cfg', 'DLSS5Preset.ini', 'reshade-shaders/Shaders/DLSS5_Feed.fx',
    'reshade-shaders/Shaders/ReShade.fxh', 'reshade-shaders/Shaders/ReShadeUI.fxh',
    'reshade-shaders/Textures/lumenite_bluenoise256.png')
$lumenite = ChildPath $PayloadDir 'reshade-shaders/Shaders/LumeniteFX'
$files += Get-ChildItem -LiteralPath $lumenite -File -Recurse | ForEach-Object { $_.FullName.Substring($PayloadDir.Length + 1) }
foreach ($relative in $files) {
    $source = ChildPath $PayloadDir $relative
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing payload: $relative" }
    if ($relative -match '\.(dll|addon64)$') {
        $stream = [IO.File]::OpenRead($source)
        $reader = [IO.BinaryReader]::new($stream)
        try {
            if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a Windows binary: $relative" }
            $stream.Position = 0x3c
            $offset = $reader.ReadInt32()
            if ($offset -lt 64 -or $offset -gt ($stream.Length - 6)) { throw "Invalid PE header: $relative" }
            $stream.Position = $offset
            if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw "Not an x64 PE: $relative" }
        } finally { $reader.Dispose() }
    }
}
foreach ($relative in @('ReShade.ini','CoD3Recomp.ini')) {
    if (!(Test-Path -LiteralPath (ChildPath $GameDir $relative))) { throw "Missing game settings: $relative" }
}
$backupRelative = 'dlss5-backup/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$backup = ChildPath $GameDir $backupRelative
New-Item -ItemType Directory -Path $backup | Out-Null
$records = foreach ($relative in @($files) + @('ReShade.ini','CoD3Recomp.ini')) {
    $destination = ChildPath $GameDir $relative
    $existed = Test-Path -LiteralPath $destination -PathType Leaf
    $originalHash = $null
    if ($existed) {
        $originalHash = (Get-FileHash -LiteralPath $destination).Hash
        $saved = ChildPath $backup $relative
        New-Item -ItemType Directory -Path (Split-Path $saved) -Force | Out-Null
        Copy-Item -LiteralPath $destination -Destination $saved
    }
    [pscustomobject]@{ path=$relative; existed=$existed; original_sha256=$originalHash }
}
# Save the recovery manifest before making the first change.
$manifest = [ordered]@{ schema=1; installed=(Get-Date -Format o); backup=$backupRelative; files=@($records); payload=@{} }
WriteUtf8 $manifestPath ($manifest | ConvertTo-Json -Depth 6)
foreach ($relative in $files) {
    $destination = ChildPath $GameDir $relative
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath (ChildPath $PayloadDir $relative) -Destination $destination -Force
    $manifest.payload[$relative] = (Get-FileHash -LiteralPath $destination).Hash
}
$iniPath = ChildPath $GameDir 'ReShade.ini'
$ini = [IO.File]::ReadAllText($iniPath)
$definition = [regex]::Match($ini, '(?m)^PreprocessorDefinitions=(.*)').Groups[1].Value.Trim()
$definition = ($definition -split ',' | Where-Object { $_ -and $_ -notmatch '^(DLSS5_MV_PROVIDER|RESHADE_DEPTH_INPUT_IS_REVERSED)=' }) -join ','
$ini = SetIni $ini 'GENERAL' 'PreprocessorDefinitions' ($definition + ',RESHADE_DEPTH_INPUT_IS_REVERSED=1,DLSS5_MV_PROVIDER=3')
$ini = SetIni $ini 'GENERAL' 'PresetPath' '.\DLSS5Preset.ini'
$ini = SetIni $ini 'GENERAL' 'SkipLoadingDisabledEffects' '1'
$ini = SetIni $ini 'OVERLAY' 'TutorialProgress' '4'
$ini = SetIni $ini 'DEPTH' 'DepthCopyBeforeClears' '0'
$ini = SetIni $ini 'DEPTH' 'UseAspectRatioHeuristics' '0'
WriteUtf8 $iniPath $ini
$settingsPath = ChildPath $GameDir 'CoD3Recomp.ini'
WriteUtf8 $settingsPath (SetIni ([IO.File]::ReadAllText($settingsPath)) 'graphics' 'antialiasing' '0')
WriteUtf8 $manifestPath ($manifest | ConvertTo-Json -Depth 6)
Write-Output "DLSS 5 installed. Backup: $backup"
