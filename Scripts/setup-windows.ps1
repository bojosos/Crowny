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

function Find-Premake {
    $repositoryPremake = Join-Path $repositoryRoot "3rdparty\premake\bin\premake5.exe"
    if (Test-Path -LiteralPath $repositoryPremake) { return $repositoryPremake }

    $command = Get-Command "premake5.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $wingetPackageRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    $candidate = Get-ChildItem -LiteralPath $wingetPackageRoot -Filter "premake5.exe" -File -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($candidate) { return $candidate.FullName }

    throw "Premake was not found in the repository or PATH."
}

function Find-SevenZip {
    $command = Get-Command "7z.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidate = Join-Path $env:ProgramFiles "7-Zip\7z.exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }

    throw "7-Zip was installed but 7z.exe could not be found."
}

function Find-MSBuild {
    $vswhere = Join-Path $repositoryRoot "3rdparty\vswhere\vswhere.exe"
    $candidate = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
    if (-not $candidate) { throw "Visual Studio 2022 Build Tools with C++ support is required." }
    return $candidate
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

function Get-MonoCompatibleFastNoiseSource {
    $source = Join-Path $repositoryRoot "Crowny\Dependencies\FastNoiseLite\CSharp\FastNoiseLite.cs"
    $contents = [IO.File]::ReadAllText($source)
    $unsupported = "private const short OPTIMISE = 512;"
    if (-not $contents.Contains($unsupported)) { return $source }

    $generatedRoot = Join-Path $dependencyRoot "generated"
    $generated = Join-Path $generatedRoot "FastNoiseLite.Mono.cs"
    New-Item -ItemType Directory -Force -Path $generatedRoot | Out-Null
    [IO.File]::WriteAllText($generated, $contents.Replace($unsupported, "private const short OPTIMISE = 0;"),
        [Text.UTF8Encoding]::new($false))
    return $generated
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

    $premake = Find-Premake
    Write-Host "Generating Visual Studio 2022 projects..."
    $premakeArguments = @("vs2022", "--with-nodes", "--simd=$($Simd.ToLowerInvariant())")
    if ($Sanitizer -eq "Address") {
        $premakeArguments += "--sanitizer=address"
    }
    Invoke-Checked -FilePath $premake -ArgumentList $premakeArguments
    $outputConfiguration = if ($Sanitizer -eq "Address") { "$Configuration-address" } else { $Configuration }

    if ($Build) {
        $msbuild = Find-MSBuild
        Write-Host "Building Crowny-Editor ($Configuration|Win64)..."
        Invoke-Checked -FilePath $msbuild -ArgumentList @(
            "Crowny-Editor\Crowny-Editor.vcxproj", "/nologo", "/v:minimal", "/m:1", "/nodeReuse:false", "/p:CL_MPCount=1",
            "/p:Configuration=$Configuration Win64", "/p:Platform=x64"
        )

        Write-Host "Building Crowny-Tests ($Configuration|Win64)..."
        Invoke-Checked -FilePath $msbuild -ArgumentList @(
            "Crowny-Tests\Crowny-Tests.vcxproj", "/nologo", "/v:minimal", "/m:1", "/nodeReuse:false", "/p:CL_MPCount=1",
            "/p:Configuration=$Configuration Win64", "/p:Platform=x64"
        )

        $assemblyRoot = Join-Path $repositoryRoot "Crowny-Editor\Resources\Assemblies"
        New-Item -ItemType Directory -Force -Path $assemblyRoot | Out-Null
        $mcs = Join-Path $monoRoot "bin\mcs.bat"
        $engineAssembly = Join-Path $repositoryRoot "Crowny-Sharp\CrownySharp.dll"
        $engineSources = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "Crowny-Sharp\Source") -Filter "*.cs" -File -Recurse |
            ForEach-Object { $_.FullName }
        $engineSources += Get-MonoCompatibleFastNoiseSource
        Write-Host "Building CrownySharp.dll..."
        Invoke-Checked -FilePath $mcs -ArgumentList (@(
            "-debug+", "-o-", "-unsafe", "-define:CROWNY_MONO", "-target:library", "-out:$engineAssembly"
        ) + $engineSources)
        Copy-Item -LiteralPath $engineAssembly, "$engineAssembly.mdb" -Destination $assemblyRoot -Force

        $gameAssembly = Join-Path $repositoryRoot "Crowny-Sandbox\GameAssembly.dll"
        $gameSources = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "Crowny-Sandbox\Source") -Filter "*.cs" -File -Recurse |
            ForEach-Object { $_.FullName }
        Write-Host "Building GameAssembly.dll..."
        Invoke-Checked -FilePath $mcs -ArgumentList (@(
            "-debug+", "-o-", "-target:library", "-lib:$(Split-Path -Parent $engineAssembly)",
            "-reference:CrownySharp.dll", "-out:$gameAssembly"
        ) + $gameSources)
        Copy-Item -LiteralPath $gameAssembly, "$gameAssembly.mdb" -Destination $assemblyRoot -Force

        $editorOutput = Join-Path $repositoryRoot "bin\$outputConfiguration-windows-x86_64\Crowny-Editor"
        Copy-Item -LiteralPath (Join-Path $vulkanRoot "Bin\shaderc_shared.dll") -Destination $editorOutput -Force
        Copy-Item -LiteralPath (Join-Path $monoRoot "bin\mono-2.0-sgen.dll") -Destination $editorOutput -Force
        Copy-Item -LiteralPath (Join-Path $openALRoot "bin\OpenAL32.dll") -Destination $editorOutput -Force

        $editorExecutable = Join-Path $editorOutput "Crowny-Editor.exe"
        Push-Location (Join-Path $repositoryRoot "Crowny-Editor")
        $originalAsanOptions = $env:ASAN_OPTIONS
        try {
            if ($Sanitizer -eq "Address") {
                $env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
            }
            Write-Host "Cooking serialized editor built-ins..."
            Invoke-Checked -FilePath $editorExecutable -ArgumentList @("--cook-builtins")
        }
        finally {
            $env:ASAN_OPTIONS = $originalAsanOptions
            Pop-Location
        }
        Invoke-Checked -FilePath "powershell" -ArgumentList @(
            "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $PSScriptRoot "pack-builtins.ps1"),
            "-RepositoryRoot", $repositoryRoot
        )
        $editorResourceOutput = Join-Path $editorOutput "Resources"
        New-Item -ItemType Directory -Force -Path $editorResourceOutput | Out-Null
        Copy-Item -LiteralPath (Join-Path $repositoryRoot "Crowny-Editor\Resources\Builtin.cwpack") -Destination $editorResourceOutput -Force

        if ($Test) {
            $testExecutable = Join-Path $repositoryRoot "bin\$outputConfiguration-windows-x86_64\Crowny-Tests\Crowny-Tests.exe"
            $originalPath = $env:PATH
            $originalAsanOptions = $env:ASAN_OPTIONS
            try {
                $env:PATH = "$(Join-Path $vulkanRoot 'Bin');$(Join-Path $monoRoot 'bin');$editorOutput;$originalPath"
                if ($Sanitizer -eq "Address") {
                    $env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
                }
                Push-Location (Join-Path $repositoryRoot "Crowny-Editor")
                Write-Host "Running Crowny-Tests ($Configuration|Win64, sanitizer: $Sanitizer)..."
                Invoke-Checked -FilePath $testExecutable -ArgumentList @()
            }
            finally {
                Pop-Location
                $env:PATH = $originalPath
                $env:ASAN_OPTIONS = $originalAsanOptions
            }
        }
    }

    Write-Host "Crowny setup completed successfully."
}
finally {
    Pop-Location
}
