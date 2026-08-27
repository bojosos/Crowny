[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")

$sccacheCommand = Get-Command "sccache.exe" -ErrorAction SilentlyContinue
if (-not $sccacheCommand) { throw "sccache.exe is not installed or is not on PATH." }

$probeRoot = Join-Path $repositoryRoot "artifacts\build-metrics\sccache-probe-$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))"
New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
$sourcePath = Join-Path $probeRoot "probe.cpp"
$projectPath = Join-Path $probeRoot "probe.vcxproj"
$cacheRoot = Join-Path $probeRoot "cache"
$wrapperPath = Join-Path $PSScriptRoot "sccache-cl.cmd"

[IO.File]::WriteAllText($sourcePath, "int CrownySccacheProbe() { return 42; }`r`n", [Text.UTF8Encoding]::new($false))
$project = @"
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{7A67C4CA-B272-4AF7-A953-E93D6B591441}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <PlatformToolset>v143</PlatformToolset>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ItemDefinitionGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'">
    <ClCompile>
      <DebugInformationFormat>OldStyle</DebugInformationFormat>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="$sourcePath" /></ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@
[IO.File]::WriteAllText($projectPath, $project, [Text.UTF8Encoding]::new($false))

$msbuild = Find-CrownyMSBuild -RepositoryRoot $repositoryRoot
$originalCacheDirectory = $env:SCCACHE_DIR
$originalServerPort = $env:SCCACHE_SERVER_PORT
$originalRealCompiler = $env:CROWNY_REAL_CL
$originalCacheBuster = $env:SCCACHE_C_CUSTOM_CACHE_BUSTER
$env:SCCACHE_DIR = $cacheRoot
$env:SCCACHE_SERVER_PORT = "$(43000 + ($PID % 1000))"
$realCompiler = Find-CrownyMSVCCompiler -RepositoryRoot $repositoryRoot
$env:CROWNY_REAL_CL = $realCompiler
$env:SCCACHE_C_CUSTOM_CACHE_BUSTER = "Crowny-MSBuild-feasibility-probe"

function Get-SccacheHits {
    $stats = (& $sccacheCommand.Source --show-stats 2>&1) | Out-String
    $matches = [regex]::Matches($stats, "Cache hits\s+(\d+)")
    if ($matches.Count -eq 0) { return 0 }
    return [int](($matches | ForEach-Object { [int]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum)
}

$passed = $false
$firstSeconds = 0.0
$secondSeconds = 0.0
$hitsAfterFirst = 0
$hitsAfterSecond = 0
try {
    & $sccacheCommand.Source --zero-stats | Out-Null

    $arguments = @(
        $projectPath, "/nologo", "/v:minimal", "/m:1", "/nodeReuse:false", "/t:Rebuild",
        "/p:Configuration=Release", "/p:Platform=x64", "/p:CL_MPCount=1",
        "/p:CLToolExe=sccache-cl.cmd", "/p:CLToolPath=$PSScriptRoot"
    )
    $timer = [Diagnostics.Stopwatch]::StartNew()
    Invoke-CrownyChecked -FilePath $msbuild -ArgumentList $arguments
    $timer.Stop()
    $firstSeconds = $timer.Elapsed.TotalSeconds
    $hitsAfterFirst = Get-SccacheHits

    $timer.Restart()
    Invoke-CrownyChecked -FilePath $msbuild -ArgumentList $arguments
    $timer.Stop()
    $secondSeconds = $timer.Elapsed.TotalSeconds
    $hitsAfterSecond = Get-SccacheHits
    $passed = $hitsAfterSecond -gt $hitsAfterFirst
}
finally {
    & $sccacheCommand.Source --stop-server 2>$null | Out-Null
    $env:SCCACHE_DIR = $originalCacheDirectory
    $env:SCCACHE_SERVER_PORT = $originalServerPort
    $env:CROWNY_REAL_CL = $originalRealCompiler
    $env:SCCACHE_C_CUSTOM_CACHE_BUSTER = $originalCacheBuster
}

$result = [ordered]@{
    passed = $passed
    firstBuildSeconds = [Math]::Round($firstSeconds, 3)
    secondBuildSeconds = [Math]::Round($secondSeconds, 3)
    hitsAfterFirst = $hitsAfterFirst
    hitsAfterSecond = $hitsAfterSecond
    compiler = $realCompiler
    sccache = $sccacheCommand.Source
    completedUtc = [DateTime]::UtcNow.ToString("o")
}
$resultPath = Join-Path $probeRoot "result.json"
$result | ConvertTo-Json | Set-Content -LiteralPath $resultPath -Encoding UTF8
if (-not $passed) { throw "sccache did not produce a cache hit through MSBuild. Result: $resultPath" }

$stampRoot = Join-Path $repositoryRoot ".deps\stamps"
New-Item -ItemType Directory -Force -Path $stampRoot | Out-Null
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stampRoot "sccache-msbuild.json") -Encoding UTF8
Write-Host "sccache MSBuild feasibility probe passed: $resultPath"
Write-Host "Keep it opt-in until a second clean rebuild reaches at least 70% hits and improves wall time by at least 30%."
