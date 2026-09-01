[CmdletBinding()]
param(
    [string]$VulkanVersion = "1.4.357.0",
    [string]$DependencyRoot = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")
$dependencyRoot = (Get-CrownyBuildRoots -RepositoryRoot $repositoryRoot -DependencyRoot $DependencyRoot).DependencyRoot
$downloadRoot = Join-Path $dependencyRoot "downloads"
$vulkanRoot = Join-Path $dependencyRoot "VulkanSDK"
$vulkanInstallerSha256 = "81F474711E9042F4CD22B31B2F7A8870DB2E428B21586FB43DD80150BE97310D"
$vmaSha256 = "8487B7995AD3B263EB73BC5B9A77D71AA69B6BEF5D58A715C02D2663AFD81F1A"

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

function Find-SevenZip {
    $command = Get-Command "7z.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidate = Join-Path $env:ProgramFiles "7-Zip\7z.exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }

    throw "7-Zip is required to extract the Vulkan SDK payload."
}

function Install-VulkanSDK {
    $header = Join-Path $vulkanRoot "Include\vulkan\vulkan.h"
    $library = Join-Path $vulkanRoot "Lib\vulkan-1.lib"
    if ((Test-Path -LiteralPath $header) -and (Test-Path -LiteralPath $library)) { return }

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
    try {
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
    }
    finally {
        $resolvedDependencyRoot = [IO.Path]::GetFullPath($dependencyRoot).TrimEnd('\') + '\'
        $resolvedContainerRoot = [IO.Path]::GetFullPath($containerRoot)
        if (-not $resolvedContainerRoot.StartsWith($resolvedDependencyRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove unexpected Vulkan staging directory: $resolvedContainerRoot"
        }
        if (Test-Path -LiteralPath $resolvedContainerRoot) {
            Remove-Item -LiteralPath $resolvedContainerRoot -Recurse -Force
        }
    }
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

Install-VulkanSDK
Install-VulkanMemoryAllocator

$requiredFiles = @(
    (Join-Path $vulkanRoot "Include\vulkan\vulkan.h"),
    (Join-Path $vulkanRoot "Include\vma\vk_mem_alloc.h"),
    (Join-Path $vulkanRoot "Lib\vulkan-1.lib")
)
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile)) { throw "Vulkan SDK setup is missing $requiredFile" }
}

$env:VULKAN_SDK = $vulkanRoot
Write-Host "Vulkan SDK $VulkanVersion is ready at $vulkanRoot."
