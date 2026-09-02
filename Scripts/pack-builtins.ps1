param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Configuration = "Release",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot "Crowny-Editor"))
$outputPath = Join-Path $sourceRoot "Resources\Builtin.cwpack"
function Test-CrownyAssetHeader([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $buffer = New-Object byte[] ([Math]::Min(512, $stream.Length))
        $count = $stream.Read($buffer, 0, $buffer.Length)
        for ($index = 0; $index -le $count - 4; $index++) {
            if ($buffer[$index] -eq 0x59 -and $buffer[$index + 1] -eq 0x4E -and
                $buffer[$index + 2] -eq 0x57 -and $buffer[$index + 3] -eq 0x43) { return $true }
        }
        return $false
    }
    finally { $stream.Dispose() }
}
$requiredCookedSources = @(
    "Resources\Icons\Play.png", "Resources\Icons\Pause.png", "Resources\Icons\Stop.png",
    "Resources\Icons\File.png", "Resources\Icons\Folder.png", "Resources\Icons\ArrowPointerIcon.png",
    "Resources\Icons\ArrowsIcon.png", "Resources\Icons\RotateIcon.png", "Resources\Icons\MaximizeIcon.png",
    "Resources\Icons\GlobeIcon.png", "Resources\Icons\SearchIcon.png", "Resources\Icons\ConsoleInfo.png",
    "Resources\Icons\ConsoleWarn.png", "Resources\Icons\ConsoleError.png", "Resources\Icons\AlignLeft.png",
    "Resources\Icons\AlignCenter.png", "Resources\Icons\AlignRight.png",
    "Resources\Fonts\Roboto\roboto-thin.ttf"
)
$cookErrors = [Collections.Generic.List[string]]::new()
foreach ($relativeSource in $requiredCookedSources) {
    $source = Join-Path $sourceRoot $relativeSource
    $isFont = [IO.Path]::GetExtension($source) -eq ".ttf"
    $asset = if ($isFont) { "$source.asset" } else { [IO.Path]::ChangeExtension($source, ".asset") }
    $relativeAsset = if ($isFont) { "$relativeSource.asset" } else { $relativeSource -replace '\.[^.]+$', '.asset' }
    if (-not (Test-Path -LiteralPath $asset)) {
        $cookErrors.Add("missing $relativeAsset")
    }
    elseif (-not (Test-CrownyAssetHeader $asset)) {
        $cookErrors.Add("legacy $relativeAsset")
    }
    elseif ((Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $asset).LastWriteTimeUtc) {
        $cookErrors.Add("stale $relativeAsset")
    }
}
$brdfAsset = Join-Path $sourceRoot "Resources\Textures\Brdf.asset"
if (-not (Test-Path -LiteralPath $brdfAsset)) { $cookErrors.Add("missing Resources/Textures/Brdf.asset") }
elseif (-not (Test-CrownyAssetHeader $brdfAsset)) { $cookErrors.Add("legacy Resources/Textures/Brdf.asset") }
Get-ChildItem -LiteralPath (Join-Path $sourceRoot "Resources\Shaders") -Filter "*.glsl" -File | ForEach-Object {
    if ((Get-Content -LiteralPath $_.FullName -Raw).Contains("#lang")) {
        $asset = [IO.Path]::ChangeExtension($_.FullName, ".asset")
        if (-not (Test-Path -LiteralPath $asset)) { $cookErrors.Add("missing Resources/Shaders/$($_.BaseName).asset") }
        elseif (-not (Test-CrownyAssetHeader $asset)) { $cookErrors.Add("legacy Resources/Shaders/$($_.BaseName).asset") }
        elseif ($_.LastWriteTimeUtc -gt (Get-Item -LiteralPath $asset).LastWriteTimeUtc) {
            $cookErrors.Add("stale Resources/Shaders/$($_.BaseName).asset")
        }
    }
}
if ($cookErrors.Count -gt 0) {
    $message = "Cooked built-ins are incomplete: $($cookErrors -join ', '). Run a Release editor once with --cook-builtins."
    if ($Check -or $Configuration -eq "Dist") { throw $message }
    Write-Warning $message
}
$patterns = @(
    "Resources\Shaders\*.asset",
    "Resources\Fonts\Roboto\roboto-thin.ttf.asset",
    "Resources\Fonts\Roboto\Roboto-Regular.ttf",
    "Resources\Fonts\Roboto\Roboto-Bold.ttf",
    "Resources\Icons\*.asset",
    "Resources\Textures\*.asset",
    "Resources\Default\*",
    "Resources\Presets\*\*.cwpreset"
)

$files = @($patterns | ForEach-Object {
    Get-ChildItem -Path (Join-Path $sourceRoot $_) -File -ErrorAction SilentlyContinue
} | Sort-Object FullName -Unique)
if ($files.Count -eq 0) {
    throw "No built-in resources found below $sourceRoot"
}

$relativePaths = @($files | ForEach-Object {
    $_.FullName.Substring($sourceRoot.Length).TrimStart('\', '/').Replace('\', '/')
})
if (Test-Path -LiteralPath $outputPath) {
    $packedPaths = @()
    $packStream = [IO.File]::OpenRead($outputPath)
    $packReader = [IO.BinaryReader]::new($packStream, [Text.Encoding]::UTF8, $true)
    try {
        $magic = [Text.Encoding]::ASCII.GetString($packReader.ReadBytes(8))
        $version = $packReader.ReadUInt32()
        $entryCount = $packReader.ReadUInt32()
        $indexOffset = $packReader.ReadUInt64()
        if ($magic -eq "CWPACK01" -and $version -eq 1 -and $entryCount -le 65536 -and $indexOffset -lt $packStream.Length) {
            $packStream.Position = [int64]$indexOffset
            for ($index = 0; $index -lt $entryCount; $index++) {
                $pathLength = $packReader.ReadUInt16()
                [void]$packReader.ReadUInt16()
                [void]$packReader.ReadUInt64()
                [void]$packReader.ReadUInt64()
                $packedPaths += [Text.Encoding]::UTF8.GetString($packReader.ReadBytes($pathLength))
            }
        }
    }
    finally {
        $packReader.Dispose()
        $packStream.Dispose()
    }

    $samePaths = $packedPaths.Count -eq $relativePaths.Count
    if ($samePaths) {
        for ($index = 0; $index -lt $relativePaths.Count; $index++) {
            if ($packedPaths[$index] -cne $relativePaths[$index]) {
                $samePaths = $false
                break
            }
        }
    }
    if ($samePaths) {
        $packWriteTime = (Get-Item -LiteralPath $outputPath).LastWriteTimeUtc
        $newestSourceTime = ($files | Measure-Object -Property LastWriteTimeUtc -Maximum).Maximum
        if ($packWriteTime -ge $newestSourceTime) {
            Write-Output "Built-in resource pack is current: $outputPath"
            exit 0
        }
    }
}

$stream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($stream, [Text.Encoding]::UTF8, $true)
$headerSize = 24
$writer.Write([byte[]]::new($headerSize))
$entries = [Collections.Generic.List[object]]::new()

foreach ($file in $files) {
    while (($stream.Position % 8) -ne 0) { $writer.Write([byte]0) }
    if (-not $file.FullName.StartsWith($sourceRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Resource is outside the editor root: $($file.FullName)"
    }
    $relative = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/').Replace('\', '/')
    $pathBytes = [Text.Encoding]::UTF8.GetBytes($relative)
    if ($pathBytes.Length -eq 0 -or $pathBytes.Length -gt 4096) {
        throw "Invalid resource path: $($file.FullName)"
    }
    $contents = [IO.File]::ReadAllBytes($file.FullName)
    $offset = $stream.Position
    $writer.Write($contents)
    $entries.Add([pscustomobject]@{ Path = $pathBytes; Offset = $offset; Size = $contents.LongLength })
}

while (($stream.Position % 8) -ne 0) { $writer.Write([byte]0) }
$indexOffset = $stream.Position
foreach ($entry in $entries) {
    $writer.Write([uint16]$entry.Path.Length)
    $writer.Write([uint16]0)
    $writer.Write([uint64]$entry.Offset)
    $writer.Write([uint64]$entry.Size)
    $writer.Write($entry.Path)
}

$stream.Position = 0
$writer.Write([Text.Encoding]::ASCII.GetBytes("CWPACK01"))
$writer.Write([uint32]1)
$writer.Write([uint32]$entries.Count)
$writer.Write([uint64]$indexOffset)
$writer.Flush()
$packed = $stream.ToArray()
$writer.Dispose()
$stream.Dispose()

$current = if (Test-Path -LiteralPath $outputPath) { [IO.File]::ReadAllBytes($outputPath) } else { $null }
$matches = $null -ne $current -and $current.Length -eq $packed.Length
if ($matches) {
    $sha256 = [Security.Cryptography.SHA256]::Create()
    $currentHash = [BitConverter]::ToString($sha256.ComputeHash($current))
    $packedHash = [BitConverter]::ToString($sha256.ComputeHash($packed))
    $sha256.Dispose()
    $matches = $currentHash -eq $packedHash
}

if ($matches) {
    Write-Output "Built-in resource pack is current: $outputPath ($($packed.Length) bytes)"
    exit 0
}
if ($Check) {
    Write-Error "Built-in resource pack is stale: $outputPath"
    exit 1
}

$temporaryPath = "$outputPath.tmp"
[IO.File]::WriteAllBytes($temporaryPath, $packed)
Move-Item -LiteralPath $temporaryPath -Destination $outputPath -Force
Write-Output "Packed $($entries.Count) resources into $outputPath ($($packed.Length) bytes)"
