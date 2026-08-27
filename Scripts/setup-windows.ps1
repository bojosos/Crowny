[CmdletBinding()]
param(
    [switch]$Build,
    [switch]$Test,
    [switch]$CoreCLR,
    [ValidateSet("Debug", "Release", "Dist")]
    [string]$Configuration = "Release",
    [ValidateSet("None", "Address")]
    [string]$Sanitizer = "None",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2",
    [ValidateRange(0, 64)]
    [int]$Jobs = 0,
    [string]$VulkanVersion = "1.4.357.0"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $repositoryRoot ".deps"
$downloadRoot = Join-Path $dependencyRoot "downloads"
$vulkanRoot = Join-Path $dependencyRoot "VulkanSDK"
$openALRoot = Join-Path $dependencyRoot "openal"
$vulkanInstallerSha256 = "81F474711E9042F4CD22B31B2F7A8870DB2E428B21586FB43DD80150BE97310D"
$vmaSha256 = "8487B7995AD3B263EB73BC5B9A77D71AA69B6BEF5D58A715C02D2663AFD81F1A"

if ($Test -and -not $Build) {
    throw "-Test requires -Build."
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string]$FilePath,
        [Parameter(Mandatory)] [string[]]$ArgumentList
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $FilePath $($ArgumentList -join ' ')"
    }
}

function Install-WinGetPackage {
    param([Parameter(Mandatory)] [string]$Id)

    $installed = & winget list --id $Id --exact --accept-source-agreements 2>$null | Out-String
    if ($LASTEXITCODE -eq 0 -and $installed -match [regex]::Escape($Id)) {
        Write-Host "$Id is already installed."
        return
    }

    Invoke-Checked -FilePath "winget" -ArgumentList @(
        "install", "--id", $Id, "--exact", "--silent",
        "--accept-package-agreements", "--accept-source-agreements",
        "--disable-interactivity"
    )
}

function Find-SevenZip {
    $command = Get-Command "7z.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidate = Join-Path $env:ProgramFiles "7-Zip\7z.exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }

    throw "7-Zip was installed but 7z.exe could not be found."
}

function Install-VulkanSDK {
    $header = Join-Path $vulkanRoot "Include\vulkan\vulkan.h"
    if (Test-Path -LiteralPath $header) { return }

    New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
    $installer = Join-Path $downloadRoot "vulkansdk-windows-X64-$VulkanVersion.exe"
    $partialInstaller = "$installer.part"
    if (Test-Path -LiteralPath $installer) {
        $existingHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
        if ($VulkanVersion -eq "1.4.357.0" -and $existingHash -ne $vulkanInstallerSha256) {
            Move-Item -LiteralPath $installer -Destination $partialInstaller -Force
        }
    }

    if (-not (Test-Path -LiteralPath $installer)) {
        $url = "https://sdk.lunarg.com/sdk/download/$VulkanVersion/windows/vulkansdk-windows-X64-$VulkanVersion.exe"
        Write-Host "Downloading Vulkan SDK $VulkanVersion..."
        Invoke-Checked -FilePath "curl.exe" -ArgumentList @(
            "--location", "--fail", "--retry", "3", "--continue-at", "-",
            "--output", $partialInstaller, $url
        )
        Move-Item -LiteralPath $partialInstaller -Destination $installer -Force
    }

    if ($VulkanVersion -eq "1.4.357.0") {
        $actualHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
        if ($actualHash -ne $vulkanInstallerSha256) { throw "The Vulkan SDK download failed checksum validation." }
    }

    Write-Host "Extracting Vulkan SDK payload into $vulkanRoot..."
    $sevenZip = Find-SevenZip
    $containerRoot = Join-Path $dependencyRoot "vulkan-package"
    $streamRoot = Join-Path $containerRoot "streams"
    New-Item -ItemType Directory -Force -Path $containerRoot, $streamRoot | Out-Null
    Invoke-Checked -FilePath $sevenZip -ArgumentList @(
        "x", "-tPE", "-y", "-o$containerRoot", $installer, "[0]"
    )
    Invoke-Checked -FilePath $sevenZip -ArgumentList @(
        "x", "-t#", "-y", "-o$streamRoot", (Join-Path $containerRoot "[0]")
    )
    New-Item -ItemType Directory -Force -Path $vulkanRoot | Out-Null
    Get-ChildItem -LiteralPath $streamRoot -Filter "*.7z" -File | ForEach-Object {
        Invoke-Checked -FilePath $sevenZip -ArgumentList @(
            "x", "-y", "-o$vulkanRoot", $_.FullName
        )
    }

    $resolvedDependencyRoot = [IO.Path]::GetFullPath($dependencyRoot).TrimEnd('\') + '\'
    $resolvedContainerRoot = [IO.Path]::GetFullPath($containerRoot)
    if (-not $resolvedContainerRoot.StartsWith($resolvedDependencyRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected Vulkan staging directory: $resolvedContainerRoot"
    }
    Remove-Item -LiteralPath $resolvedContainerRoot -Recurse -Force
}

function Install-VulkanMemoryAllocator {
    $vmaHeader = Join-Path $vulkanRoot "Include\vma\vk_mem_alloc.h"
    if (Test-Path -LiteralPath $vmaHeader) {
        $actualHash = (Get-FileHash -LiteralPath $vmaHeader -Algorithm SHA256).Hash
        if ($actualHash -eq $vmaSha256) { return }
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $vmaHeader) | Out-Null
    $url = "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/v3.4.0/include/vk_mem_alloc.h"
    Invoke-Checked -FilePath "curl.exe" -ArgumentList @(
        "--location", "--fail", "--retry", "3", "--output", $vmaHeader, $url
    )
    $actualHash = (Get-FileHash -LiteralPath $vmaHeader -Algorithm SHA256).Hash
    if ($actualHash -ne $vmaSha256) { throw "The Vulkan Memory Allocator download failed checksum validation." }
}

if (-not (Get-Command "winget" -ErrorAction SilentlyContinue)) {
    throw "Windows Package Manager (winget) is required to bootstrap Mono, 7-Zip, and CMake."
}

Push-Location $repositoryRoot
try {
    Write-Host "Initializing Git submodules..."
    Invoke-Checked -FilePath "git" -ArgumentList @("submodule", "sync", "--recursive")
    Invoke-Checked -FilePath "git" -ArgumentList @("submodule", "update", "--init", "--recursive")

    if ($CoreCLR) {
        & (Join-Path $PSScriptRoot "setup-dotnet.ps1")
    }

    Install-WinGetPackage -Id "Mono.Mono"
    Install-WinGetPackage -Id "7zip.7zip"
    Install-WinGetPackage -Id "Kitware.CMake"
    Install-VulkanSDK
    Install-VulkanMemoryAllocator
    $physicsConfiguration = if ($Configuration -eq "Debug") { "Debug" } else { "Release" }
    & (Join-Path $PSScriptRoot "setup-openal.ps1") -Configuration $physicsConfiguration -Simd $Simd

    & (Join-Path $PSScriptRoot "setup-physics.ps1") -Configuration $physicsConfiguration -Simd $Simd
    & (Join-Path $PSScriptRoot "setup-spirv-cross.ps1") -Configuration $physicsConfiguration -VulkanVersion $VulkanVersion -Simd $Simd

    $monoRoot = Join-Path $env:ProgramFiles "Mono"
    $requiredFiles = @(
        (Join-Path $monoRoot "include\mono-2.0\mono\jit\jit.h"),
        (Join-Path $monoRoot "lib\mono-2.0-sgen.lib"),
        (Join-Path $vulkanRoot "Include\vulkan\vulkan.h"),
        (Join-Path $vulkanRoot "Include\vma\vk_mem_alloc.h"),
        (Join-Path $vulkanRoot "Lib\vulkan-1.lib"),
        (Join-Path $openALRoot "lib\OpenAL32.lib"),
        (Join-Path $openALRoot "bin\OpenAL32.dll")
    )
    $spirvCrossPostfix = if ($physicsConfiguration -eq "Debug") { "d" } else { "" }
    $requiredFiles += @(
        (Join-Path $dependencyRoot "spirv-cross\install\$physicsConfiguration\lib\spirv-cross-core$spirvCrossPostfix.lib"),
        (Join-Path $dependencyRoot "spirv-cross\install\$physicsConfiguration\lib\spirv-cross-glsl$spirvCrossPostfix.lib")
    )
    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile)) { throw "Missing dependency file: $requiredFile" }
    }

    $env:CROWNY_MONO_ROOT = $monoRoot
    $env:VULKAN_SDK = $vulkanRoot
    $env:CROWNY_OPENAL_ROOT = $openALRoot
    $env:CROWNY_PHYSICS_ROOT = Join-Path $dependencyRoot "physics\install"
    $env:CROWNY_SPIRV_CROSS_ROOT = Join-Path $dependencyRoot "spirv-cross\install"

    . (Join-Path $PSScriptRoot "windows-build-common.ps1")
    Ensure-CrownyProjects -RepositoryRoot $repositoryRoot -Simd $Simd -Force

    if ($Build) {
        if ($Sanitizer -eq "None") {
            & (Join-Path $PSScriptRoot "build-windows.ps1") -Target Editor -Configuration $Configuration -Jobs $Jobs -Simd $Simd
        }

        if ($Test) {
            & (Join-Path $PSScriptRoot "test-windows.ps1") -Configuration $Configuration -Sanitizer $Sanitizer -Jobs $Jobs -Simd $Simd
        }
        else {
            & (Join-Path $PSScriptRoot "build-windows.ps1") -Target Tests -Configuration $Configuration -Sanitizer $Sanitizer -Jobs $Jobs -Simd $Simd
        }
    }

    Write-Host "Crowny setup completed successfully."
}
finally {
    Pop-Location
}
