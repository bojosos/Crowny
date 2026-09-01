[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2",
    [string]$DependencyRoot = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")
$dependencyRoot = (Get-CrownyBuildRoots -RepositoryRoot $repositoryRoot -DependencyRoot $DependencyRoot).DependencyRoot
$sourceRoot = Join-Path $repositoryRoot "Crowny\Dependencies\openal-soft"
$openALRoot = Join-Path $dependencyRoot "openal"
$buildRoot = Join-Path $openALRoot "build"
$library = Join-Path $openALRoot "lib\OpenAL32.lib"
$runtime = Join-Path $openALRoot "bin\OpenAL32.dll"
$header = Join-Path $openALRoot "include\AL\al.h"
$stamp = Join-Path $openALRoot ".crowny-openal-version"
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

if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot "CMakeLists.txt"))) {
    throw "The OpenAL Soft submodule is missing. Run git submodule update --init --recursive."
}
if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) { throw "Git is required." }

$sourceCommit = & git -C $sourceRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or -not $sourceCommit) {
    throw "Could not identify the OpenAL Soft source revision."
}
$sourceCommit = $sourceCommit.Trim()
$expectedStamp = "commit=$sourceCommit`nconfiguration=$Configuration`nruntime=dynamic-v1`nsimd=$simdLevel-v1"

if (-not $Force -and (Test-Path -LiteralPath $stamp) -and
    (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $expectedStamp.Trim() -and
    (Test-Path -LiteralPath $library) -and (Test-Path -LiteralPath $runtime) -and
    (Test-Path -LiteralPath $header)) {
    Write-Host "OpenAL Soft is already built for $Configuration with $Simd."
    return
}

$cmake = Find-CMake
New-Item -ItemType Directory -Force -Path $buildRoot, $openALRoot | Out-Null
$configureArguments = @(
    "-S", $sourceRoot, "-B", $buildRoot, "-A", "x64",
    "-DCMAKE_INSTALL_PREFIX=$openALRoot",
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
    "-DLIBTYPE=SHARED",
    "-DALSOFT_UTILS=OFF", "-DALSOFT_EXAMPLES=OFF", "-DALSOFT_NO_CONFIG_UTIL=ON",
    "-DALSOFT_INSTALL_CONFIG=OFF", "-DALSOFT_INSTALL_HRTF_DATA=OFF",
    "-DALSOFT_INSTALL_AMBDEC_PRESETS=OFF", "-DALSOFT_REQUIRE_SSE4_1=ON"
) + $simdCMakeOptions
Invoke-Checked -FilePath $cmake -ArgumentList $configureArguments
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "--build", $buildRoot, "--config", $Configuration, "--target", "install", "--parallel"
)

$missingFiles = @($library, $runtime, $header) | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingFiles) { throw "Missing OpenAL Soft files after build: $($missingFiles -join ', ')" }
Set-Content -LiteralPath $stamp -Value $expectedStamp
Write-Host "OpenAL Soft is ready in $openALRoot."
