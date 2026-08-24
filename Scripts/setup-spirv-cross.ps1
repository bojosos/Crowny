[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$VulkanVersion = "1.4.357.0",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $repositoryRoot ".deps\spirv-cross"
$sourceRoot = Join-Path $dependencyRoot "source"
$buildRoot = Join-Path $dependencyRoot "build\$Configuration"
$installRoot = Join-Path $dependencyRoot "install\$Configuration"
$tag = "vulkan-sdk-$VulkanVersion"
$simdLevel = $Simd.ToLowerInvariant()
$simdCMakeOptions = if ($Simd -eq "AVX2") {
    @("-DCMAKE_C_FLAGS=/arch:AVX2", "-DCMAKE_CXX_FLAGS=/arch:AVX2")
} else {
    @()
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

function Find-CMake {
    $command = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        (Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "CMake 3.22 or newer is required."
}

function Assert-ChildPath {
    param([Parameter(Mandatory)] [string]$Path)

    $root = [IO.Path]::GetFullPath($dependencyRoot).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath($Path)
    if (-not $candidate.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside $dependencyRoot`: $candidate"
    }
}

function Test-ExpectedSource {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot ".git"))) { return $false }
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot "spirv_glsl.cpp"))) { return $false }

    $head = & git -C $sourceRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0) { return $false }
    $tagCommit = & git -C $sourceRoot rev-parse "$tag^{commit}" 2>$null
    return $LASTEXITCODE -eq 0 -and $head.Trim() -eq $tagCommit.Trim()
}

if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) { throw "Git is required." }
$cmake = Find-CMake

New-Item -ItemType Directory -Force -Path $dependencyRoot | Out-Null
if (-not (Test-ExpectedSource)) {
    $stagingRoot = Join-Path $dependencyRoot "staging-source"
    Assert-ChildPath -Path $sourceRoot
    Assert-ChildPath -Path $stagingRoot
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }

    Write-Host "Fetching SPIRV-Cross $tag..."
    Invoke-Checked -FilePath "git" -ArgumentList @(
        "clone", "--depth", "1", "--branch", $tag,
        "https://github.com/KhronosGroup/SPIRV-Cross.git", $stagingRoot
    )
    if (Test-Path -LiteralPath $sourceRoot) {
        Remove-Item -LiteralPath $sourceRoot -Recurse -Force
    }
    Move-Item -LiteralPath $stagingRoot -Destination $sourceRoot
    if (-not (Test-ExpectedSource)) { throw "SPIRV-Cross source validation failed." }
}

$sourceCommit = (& git -C $sourceRoot rev-parse HEAD).Trim()
$stamp = Join-Path $installRoot ".crowny-spirv-cross-version"
$expectedStamp = "tag=$tag`ncommit=$sourceCommit`nconfiguration=$Configuration`nruntime=dynamic-v1`nsimd=$simdLevel-v1"
$debugPostfix = if ($Configuration -eq "Debug") { "d" } else { "" }
$requiredLibraries = @(
    (Join-Path $installRoot "lib\spirv-cross-core$debugPostfix.lib"),
    (Join-Path $installRoot "lib\spirv-cross-glsl$debugPostfix.lib")
)
if (-not $Force -and (Test-Path -LiteralPath $stamp) -and
    (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $expectedStamp.Trim() -and
    -not ($requiredLibraries | Where-Object { -not (Test-Path -LiteralPath $_) })) {
    Write-Host "SPIRV-Cross is already built for $Configuration."
    return
}

New-Item -ItemType Directory -Force -Path $buildRoot, $installRoot | Out-Null
$configureArguments = @(
    "-S", $sourceRoot, "-B", $buildRoot, "-A", "x64",
    "-DCMAKE_INSTALL_PREFIX=$installRoot",
    "-DCMAKE_DEBUG_POSTFIX=d",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
    "-DSPIRV_CROSS_STATIC=ON", "-DSPIRV_CROSS_SHARED=OFF", "-DSPIRV_CROSS_CLI=OFF",
    "-DSPIRV_CROSS_ENABLE_TESTS=OFF", "-DSPIRV_CROSS_ENABLE_GLSL=ON",
    "-DSPIRV_CROSS_ENABLE_HLSL=OFF", "-DSPIRV_CROSS_ENABLE_MSL=OFF",
    "-DSPIRV_CROSS_ENABLE_CPP=OFF", "-DSPIRV_CROSS_ENABLE_REFLECT=OFF",
    "-DSPIRV_CROSS_ENABLE_C_API=OFF", "-DSPIRV_CROSS_ENABLE_UTIL=OFF"
) + $simdCMakeOptions
Invoke-Checked -FilePath $cmake -ArgumentList $configureArguments
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "--build", $buildRoot, "--config", $Configuration, "--target", "install", "--parallel"
)

$missingLibraries = $requiredLibraries | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingLibraries) { throw "Missing SPIRV-Cross libraries after build: $($missingLibraries -join ', ')" }
Set-Content -LiteralPath $stamp -Value $expectedStamp
Write-Host "SPIRV-Cross is ready in $installRoot."
