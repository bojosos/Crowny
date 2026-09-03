[CmdletBinding()]
param(
    [string]$VulkanVersion = "1.4.357.0",
    [string]$DependencyRoot = ""
)

$ErrorActionPreference = "Stop"
$arguments = @("deps", "vulkan", "--version", $VulkanVersion)
if ($DependencyRoot) { $env:CROWNY_DEPS_ROOT = $DependencyRoot }
& (Join-Path $PSScriptRoot "crowny.bat") @arguments
exit $LASTEXITCODE
