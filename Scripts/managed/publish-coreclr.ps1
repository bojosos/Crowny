[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RuntimeIdentifier,

    [string] $RuntimeVersion = "",

    [string] $RuntimeRoot = "",

    [Parameter(Mandatory = $true)]
    [string] $GameProject,

    [Parameter(Mandatory = $true)]
    [string] $GameAssemblyName,

    [Parameter(Mandatory = $true)]
    [string] $OutputDirectory,

    [ValidateSet("Debug", "Release")]
    [string] $Configuration = "Release",

    [string] $DotNetExecutable = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$repositoryDotNetRoot = Join-Path $repositoryRoot ".deps\dotnet"
$repositoryDotNetName = if ($env:OS -eq "Windows_NT") { "dotnet.exe" } else { "dotnet" }
$repositoryDotNet = Join-Path $repositoryDotNetRoot $repositoryDotNetName
$runtimeRootWasDefaulted = [string]::IsNullOrWhiteSpace($RuntimeRoot)
if ([string]::IsNullOrWhiteSpace($DotNetExecutable)) {
    if (-not (Test-Path -LiteralPath $repositoryDotNet -PathType Leaf)) {
        & (Join-Path $repositoryRoot "Scripts\setup-dotnet.ps1")
    }
    $DotNetExecutable = $repositoryDotNet
}
if ([string]::IsNullOrWhiteSpace($RuntimeRoot)) {
    $RuntimeRoot = $repositoryDotNetRoot
}

function Get-HostRuntimeIdentifier {
    $platform = if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) {
        "win"
    } elseif ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
        "linux"
    } elseif ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::OSX)) {
        "osx"
    } else {
        throw "CoreCLR packaging is not supported on this host operating system."
    }
    $architecture = [Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString().ToLowerInvariant()
    return "$platform-$architecture"
}

$hostRuntimeIdentifier = Get-HostRuntimeIdentifier
if ($runtimeRootWasDefaulted -and $RuntimeIdentifier -ne $hostRuntimeIdentifier) {
    throw "RuntimeIdentifier '$RuntimeIdentifier' does not match this host ($hostRuntimeIdentifier). Supply a matching -RuntimeRoot for cross-architecture packaging."
}
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
if ([string]::IsNullOrWhiteSpace($RuntimeVersion)) {
    $runtimeDirectory = Join-Path $runtimeRootPath "shared\Microsoft.NETCore.App"
    $fxrDirectory = Join-Path $runtimeRootPath "host\fxr"
    $candidates = if (Test-Path -LiteralPath $runtimeDirectory) {
        Get-ChildItem -LiteralPath $runtimeDirectory -Directory | Where-Object {
            Test-Path -LiteralPath (Join-Path $fxrDirectory $_.Name) -PathType Container
        }
    } else { @() }
    $selectedRuntime = $candidates | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if ($null -eq $selectedRuntime) {
        throw "Private runtime root has no matching host/fxr and Microsoft.NETCore.App versions: $runtimeRootPath"
    }
    $RuntimeVersion = $selectedRuntime.Name
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

$dotnetCommand = Get-Command $DotNetExecutable -ErrorAction Stop
$dotnetRoot = Split-Path -Parent $dotnetCommand.Source
$repositoryNuGetRoot = Join-Path $repositoryRoot ".deps\nuget-packages"
$hostPackId = "Microsoft.NETCore.App.Host.$RuntimeIdentifier"
$hostPackRoot = Join-Path $repositoryNuGetRoot "$($hostPackId.ToLowerInvariant())\$RuntimeVersion"
$hostPackProjectRoot = Join-Path $repositoryRoot ".deps\managed-host-pack"
$hostPackProject = Join-Path $hostPackProjectRoot "restore.csproj"
$netHostCandidates = @(
    (Join-Path $dotnetRoot "packs\Microsoft.NETCore.App.Host.$RuntimeIdentifier\$RuntimeVersion\runtimes\$RuntimeIdentifier\native\$netHostName"),
    (Join-Path $hostPackRoot "runtimes\$RuntimeIdentifier\native\$netHostName")
)
$netHostSource = $netHostCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ($null -eq $netHostSource) {
    New-Item -ItemType Directory -Force -Path $hostPackProjectRoot, $repositoryNuGetRoot | Out-Null
    $restoreProjectXml = @"
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <RuntimeIdentifier>$RuntimeIdentifier</RuntimeIdentifier>
  </PropertyGroup>
  <ItemGroup>
    <PackageDownload Include="$hostPackId" Version="[$RuntimeVersion]" />
  </ItemGroup>
</Project>
"@
    [IO.File]::WriteAllText($hostPackProject, $restoreProjectXml, [Text.UTF8Encoding]::new($false))
    & $DotNetExecutable restore $hostPackProject --packages $repositoryNuGetRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Restoring $hostPackId $RuntimeVersion failed with exit code $LASTEXITCODE."
    }
    $netHostSource = $netHostCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if ($null -eq $netHostSource) {
        throw "Matching $netHostName was not found after restoring $hostPackId $RuntimeVersion."
    }
}
Copy-Item -LiteralPath $netHostSource -Destination $hostOutput

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
