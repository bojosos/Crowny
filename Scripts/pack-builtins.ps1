param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Configuration = "Release",
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$arguments = @((Join-Path $PSScriptRoot "pack-builtins.py"), "--repo-root", $RepositoryRoot, "--configuration", $Configuration)
if ($Check) { $arguments += "--check" }
& python @arguments
exit $LASTEXITCODE
