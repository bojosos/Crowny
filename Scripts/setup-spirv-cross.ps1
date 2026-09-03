[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$VulkanVersion = "1.4.357.0",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2",
    [switch]$Force,
    [string]$DependencyRoot = ""
)

$ErrorActionPreference = "Stop"
$arguments = @("deps", "spirv-cross", "--configuration", $Configuration, "--vulkan-version", $VulkanVersion, "--simd", $Simd.ToLowerInvariant())
if ($Force) { $arguments += "--force" }
if ($DependencyRoot) { $env:CROWNY_DEPS_ROOT = $DependencyRoot }
& (Join-Path $PSScriptRoot "crowny.bat") @arguments
exit $LASTEXITCODE
