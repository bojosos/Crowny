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
$vulkanRoot = Join-Path $dependencyRoot "VulkanSDK"
$openALRoot = Join-Path $dependencyRoot "openal"

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
    & (Join-Path $PSScriptRoot "setup-vulkan.ps1") -VulkanVersion $VulkanVersion
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
