[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "Dist")]
    [string]$Configuration = "Release",
    [ValidateSet("All", "Vulkan", "OpenGL")]
    [string]$Backend = "All",
    [ValidateSet("None", "Address")]
    [string]$Sanitizer = "None",
    [ValidateRange(0, 64)]
    [int]$Jobs = 0,
    [string]$Filter = "",
    [string]$ReferenceRoot = "Crowny-RenderTests/References",
    [string]$ArtifactRoot = "artifacts/render-tests",
    [switch]$UpdateReferences,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$arguments = @("render-tests", "--configuration", $Configuration, "--backend", $Backend, "--sanitizer", $Sanitizer, "--jobs", "$Jobs", "--reference-root", $ReferenceRoot, "--artifact-root", $ArtifactRoot)
if ($Filter) { $arguments += @("--filter", $Filter) }
if ($UpdateReferences) { $arguments += "--update-references" }
if ($NoBuild) { $arguments += "--no-build" }
& (Join-Path $PSScriptRoot "crowny.bat") @arguments
exit $LASTEXITCODE
