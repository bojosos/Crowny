[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$InstallDirectory = "",
    [ValidateSet("x64", "arm64")]
    [string]$Architecture = "x64"
)

$ErrorActionPreference = "Stop"
$arguments = @("deps", "dotnet", "--architecture", $Architecture)
if ($Version) { $arguments += @("--version", $Version) }
if ($InstallDirectory) { $arguments += @("--install-dir", $InstallDirectory) }
& (Join-Path $PSScriptRoot "crowny.bat") @arguments
exit $LASTEXITCODE
