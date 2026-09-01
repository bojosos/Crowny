[CmdletBinding()]
param(
    [string] $Version = "",
    [string] $InstallDirectory = "",
    [string] $DependencyRoot = "",
    [ValidateSet("x64", "arm64")]
    [string] $Architecture = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
. (Join-Path $PSScriptRoot "windows-build-common.ps1")
$dependencyRoot = (Get-CrownyBuildRoots -RepositoryRoot $repositoryRoot -DependencyRoot $DependencyRoot).DependencyRoot
$downloadRoot = Join-Path $dependencyRoot "downloads"
$globalJsonPath = Join-Path $repositoryRoot "global.json"
$installerSha256 = "E8B873E18A81E5C4CD8AB69D84DAC8FEAD291D50B3C44633CD7FDDAD709A13D6"

function Get-PlatformPrefix {
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) { return "win" }
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) { return "linux" }
    if ([Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::OSX)) { return "osx" }
    throw "The repository .NET bootstrap does not support this operating system."
}

if (-not (Test-Path -LiteralPath $globalJsonPath -PathType Leaf)) {
    throw "The repository SDK pin is missing: $globalJsonPath"
}
$globalJson = Get-Content -LiteralPath $globalJsonPath -Raw | ConvertFrom-Json
$pinnedVersion = [string]$globalJson.sdk.version
if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = $pinnedVersion
}
elseif ($Version -ne $pinnedVersion) {
    throw "Requested .NET SDK $Version does not match global.json ($pinnedVersion)."
}

if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $installName = if ($Architecture -eq "x64") { "dotnet" } else { "dotnet-$Architecture" }
    $InstallDirectory = Join-Path $dependencyRoot $installName
}
$installRoot = [IO.Path]::GetFullPath($InstallDirectory)
$dotnetName = if ($env:OS -eq "Windows_NT") { "dotnet.exe" } else { "dotnet" }
$dotnet = Join-Path $installRoot $dotnetName
$expectedRuntimeIdentifier = "$(Get-PlatformPrefix)-$Architecture"

function Test-InstalledSdk {
    if (-not (Test-Path -LiteralPath $dotnet -PathType Leaf)) { return $false }
    $installed = & $dotnet --version 2>$null
    if ($LASTEXITCODE -ne 0 -or $installed.Trim() -ne $Version) { return $false }
    $versionMetadata = Join-Path $installRoot "sdk\$Version\.version"
    if (-not (Test-Path -LiteralPath $versionMetadata -PathType Leaf)) { return $false }
    return (Get-Content -LiteralPath $versionMetadata) -contains $expectedRuntimeIdentifier
}

if (-not (Test-InstalledSdk)) {
    New-Item -ItemType Directory -Force -Path $downloadRoot, $installRoot | Out-Null
    $installer = Join-Path $downloadRoot "dotnet-install.ps1"
    $partial = "$installer.part"
    if (Test-Path -LiteralPath $installer -PathType Leaf) {
        $cachedHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
        if ($cachedHash -ne $installerSha256) {
            throw "The cached dotnet-install.ps1 does not match the repository-pinned checksum. Remove $installer and review the new installer before updating the pin."
        }
    }
    else {
        Write-Host "Downloading Microsoft's .NET install script..."
        $curl = Get-Command "curl.exe" -ErrorAction SilentlyContinue
        if ($curl) {
            & $curl.Source --location --fail --retry 3 --output $partial "https://dot.net/v1/dotnet-install.ps1"
            if ($LASTEXITCODE -ne 0) { throw "Downloading dotnet-install.ps1 failed with exit code $LASTEXITCODE." }
        }
        else {
            Invoke-WebRequest -Uri "https://dot.net/v1/dotnet-install.ps1" -OutFile $partial -UseBasicParsing
        }
        $downloadedHash = (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash
        if ($downloadedHash -ne $installerSha256) {
            Remove-Item -LiteralPath $partial -Force
            throw "The downloaded dotnet-install.ps1 does not match the repository-pinned checksum. Review the upstream change before updating the pin."
        }
        Move-Item -LiteralPath $partial -Destination $installer -Force
    }

    Write-Host "Installing .NET SDK $Version ($Architecture) into $installRoot..."
    & $installer -Version $Version -InstallDir $installRoot -Architecture $Architecture -NoPath
    if ($LASTEXITCODE -ne 0) { throw "dotnet-install.ps1 failed with exit code $LASTEXITCODE." }
}

if (-not (Test-InstalledSdk)) {
    throw "The repository-local .NET SDK did not install correctly at $dotnet."
}

$runtimeRoot = Join-Path $installRoot "shared\Microsoft.NETCore.App"
$hostFxrRoot = Join-Path $installRoot "host\fxr"
$runtimeVersions = if (Test-Path -LiteralPath $runtimeRoot) {
    Get-ChildItem -LiteralPath $runtimeRoot -Directory | Select-Object -ExpandProperty Name
} else { @() }
$hostVersions = if (Test-Path -LiteralPath $hostFxrRoot) {
    Get-ChildItem -LiteralPath $hostFxrRoot -Directory | Select-Object -ExpandProperty Name
} else { @() }
$privateRuntimeVersions = @($runtimeVersions | Where-Object { $hostVersions -contains $_ })
if ($privateRuntimeVersions.Count -eq 0) {
    throw "The SDK install has no matching host/fxr and Microsoft.NETCore.App runtime directories."
}

Write-Host "Repository .NET SDK ready: $dotnet"
Write-Host "Private runtime versions: $($privateRuntimeVersions -join ', ')"
