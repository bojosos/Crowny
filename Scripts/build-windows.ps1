[CmdletBinding()]
param(
    [ValidateSet("Engine", "Editor", "Tests", "RenderTests", "All")]
    [string]$Target = "Engine",
    [ValidateSet("Debug", "Release", "Dist")]
    [string]$Configuration = "Release",
    [ValidateSet("None", "Address")]
    [string]$Sanitizer = "None",
    [ValidateRange(0, 64)]
    [int]$Jobs = 0,
    [switch]$Clean,
    [switch]$Profile,
    [ValidateSet("None", "Sccache")]
    [string]$CompilerCache = "None",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2"
)

$ErrorActionPreference = "Stop"
$arguments = @("build", "--target", $Target, "--configuration", $Configuration, "--sanitizer", $Sanitizer, "--jobs", "$Jobs", "--compiler-cache", $CompilerCache, "--simd", $Simd.ToLowerInvariant())
if ($Clean) { $arguments += "--clean" }
if ($Profile) { $arguments += "--profile" }
& (Join-Path $PSScriptRoot "crowny.bat") @arguments
exit $LASTEXITCODE
