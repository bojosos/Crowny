[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RuntimeIdentifier,

    [Parameter(Mandatory = $true)]
    [string] $RuntimeVersion,

    [Parameter(Mandatory = $true)]
    [string] $RuntimeRoot,

    [Parameter(Mandatory = $true)]
    [string] $GameProject,

    [Parameter(Mandatory = $true)]
    [string] $GameAssemblyName,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory,

    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Release",

    [string] $DotNetExecutable = "dotnet"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$runtimeRootPath = [IO.Path]::GetFullPath($RuntimeRoot)
$gameProjectPath = [IO.Path]::GetFullPath($GameProject)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$hostProject = Join-Path $repositoryRoot "Crowny-Managed\Crowny.ManagedHost\Crowny.ManagedHost.csproj"

if (-not (Test-Path -LiteralPath $hostProject -PathType Leaf)) {
    throw "Managed host project is missing: $hostProject"
}
if (-not (Test-Path -LiteralPath $gameProjectPath -PathType Leaf)) {
    throw "Managed game project is missing: $gameProjectPath"
}
if (-not (Test-Path -LiteralPath $runtimeRootPath -PathType Container)) {
    throw "Private runtime root is missing: $runtimeRootPath"
}
if (Test-Path -LiteralPath $outputPath) {
    $existingOutput = Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1
    if ($null -ne $existingOutput) {
        throw "Managed publish output must be empty: $outputPath"
    }
}

$dotnetHostName = if ($RuntimeIdentifier.StartsWith("win-", [StringComparison]::Ordinal)) { "dotnet.exe" } else { "dotnet" }
$netHostName = if ($RuntimeIdentifier.StartsWith("win-", [StringComparison]::Ordinal)) {
    "nethost.dll"
} elseif ($RuntimeIdentifier.StartsWith("linux-", [StringComparison]::Ordinal)) {
    "libnethost.so"
} elseif ($RuntimeIdentifier.StartsWith("osx-", [StringComparison]::Ordinal)) {
    "libnethost.dylib"
} else {
    throw "CoreCLR packaging is not defined for runtime identifier '$RuntimeIdentifier'."
}

$dotnetHost = Join-Path $runtimeRootPath $dotnetHostName
$hostFxrDirectory = Join-Path $runtimeRootPath "host\fxr\$RuntimeVersion"
$sharedRuntimeDirectory = Join-Path $runtimeRootPath "shared\Microsoft.NETCore.App\$RuntimeVersion"
if (-not (Test-Path -LiteralPath $dotnetHost -PathType Leaf) -or
    -not (Test-Path -LiteralPath $hostFxrDirectory -PathType Container) -or
    -not (Test-Path -LiteralPath $sharedRuntimeDirectory -PathType Container)) {
    throw "Runtime root does not contain dotnet, hostfxr $RuntimeVersion, and Microsoft.NETCore.App $RuntimeVersion."
}

$hostOutput = Join-Path $outputPath "host"
$gameOutput = Join-Path $outputPath "game"
$privateRuntimeOutput = Join-Path $outputPath "runtime"
New-Item -ItemType Directory -Path $hostOutput, $gameOutput, $privateRuntimeOutput -Force | Out-Null

& $DotNetExecutable publish $hostProject --configuration $Configuration --framework net10.0 --runtime $RuntimeIdentifier `
    --self-contained false --output $hostOutput -p:DotNetRuntimeVersion=$RuntimeVersion
if ($LASTEXITCODE -ne 0) {
    throw "Managed host publish failed with exit code $LASTEXITCODE."
}

& $DotNetExecutable publish $gameProjectPath --configuration $Configuration --framework net10.0 --runtime $RuntimeIdentifier `
    --self-contained false --output $gameOutput -p:UseAppHost=false
if ($LASTEXITCODE -ne 0) {
    throw "Managed game publish failed with exit code $LASTEXITCODE."
}

Copy-Item -LiteralPath $dotnetHost -Destination $privateRuntimeOutput
New-Item -ItemType Directory -Path (Join-Path $privateRuntimeOutput "host\fxr"),
                                  (Join-Path $privateRuntimeOutput "shared\Microsoft.NETCore.App") -Force | Out-Null
Copy-Item -LiteralPath $hostFxrDirectory -Destination (Join-Path $privateRuntimeOutput "host\fxr") -Recurse
Copy-Item -LiteralPath $sharedRuntimeDirectory -Destination (Join-Path $privateRuntimeOutput "shared\Microsoft.NETCore.App") -Recurse
foreach ($notice in @("LICENSE.txt", "ThirdPartyNotices.txt")) {
    $noticePath = Join-Path $runtimeRootPath $notice
    if (Test-Path -LiteralPath $noticePath -PathType Leaf) {
        Copy-Item -LiteralPath $noticePath -Destination $privateRuntimeOutput
    }
}

$requiredArtifacts = @(
    (Join-Path $hostOutput "Crowny.ManagedHost.dll"),
    (Join-Path $hostOutput "Crowny.ManagedHost.deps.json"),
    (Join-Path $hostOutput "Crowny.ManagedHost.runtimeconfig.json"),
    (Join-Path $hostOutput $netHostName),
    (Join-Path $gameOutput "$GameAssemblyName.dll"),
    (Join-Path $gameOutput "$GameAssemblyName.deps.json")
)
foreach ($artifact in $requiredArtifacts) {
    if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
        throw "Managed publish artifact is missing: $artifact"
    }
}

$manifest = [ordered]@{
    schemaVersion = 1
    abiVersion = 1
    backend = "CoreCLR"
    runtimeIdentifier = $RuntimeIdentifier
    runtimeVersion = $RuntimeVersion
    runtimeRoot = "runtime"
    artifacts = [ordered]@{
        nethost = "host/$netHostName"
        hostAssembly = "host/Crowny.ManagedHost.dll"
        hostDependencies = "host/Crowny.ManagedHost.deps.json"
        runtimeConfig = "host/Crowny.ManagedHost.runtimeconfig.json"
        gameAssembly = "game/$GameAssemblyName.dll"
        gameDependencies = "game/$GameAssemblyName.deps.json"
    }
}
$manifestJson = $manifest | ConvertTo-Json -Depth 4
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $outputPath "managed-program.json"), $manifestJson, $utf8NoBom)
