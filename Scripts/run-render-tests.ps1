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
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildCommon = Join-Path $PSScriptRoot "windows-build-common.ps1"
. $buildCommon

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

$outputReadLock = $null
$runtimeLock = $null
Push-Location $repositoryRoot
try {
    if (-not $NoBuild) {
        & (Join-Path $PSScriptRoot "build-windows.ps1") -Target RenderTests -Configuration $Configuration -Sanitizer $Sanitizer -Jobs $Jobs
    }

    $workspaceConfiguration = Get-CrownyWorkspaceConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
    $outputReadLock = Enter-CrownyOutputReadLock -RepositoryRoot $repositoryRoot -Configuration $workspaceConfiguration -Wait
    $runtimeLock = Enter-CrownyExclusiveLock -RepositoryRoot $repositoryRoot -Name "render-test-runtime" -Wait

    $outputConfiguration = if ($Sanitizer -eq "Address") { "$Configuration-address" } else { $Configuration }
    $executable = Join-Path $repositoryRoot "bin\$outputConfiguration-windows-x86_64\Crowny-RenderTests\Crowny-RenderTests.exe"
    if (-not (Test-Path -LiteralPath $executable)) { throw "Render test executable was not found: $executable" }

    $selectedBackends = switch ($Backend) {
        "Vulkan" { @("vulkan") }
        "OpenGL" { @("opengl") }
        default { @("vulkan", "opengl") }
    }
    $resolvedReferences = Join-Path $repositoryRoot $ReferenceRoot
    $resolvedArtifacts = Join-Path $repositoryRoot $ArtifactRoot

    if ($UpdateReferences) {
        if ($Backend -eq "OpenGL") { throw "Reference updates use Vulkan. Select Vulkan or All." }
        $updateArguments = @(
            "--backend", "vulkan", "--references", $resolvedReferences, "--artifacts", $resolvedArtifacts,
            "--update-references"
        )
        if ($Filter) { $updateArguments += @("--filter", $Filter) }
        Invoke-Checked -FilePath $executable -ArgumentList $updateArguments
    }

    foreach ($renderer in $selectedBackends) {
        $arguments = @(
            "--backend", $renderer, "--references", $resolvedReferences, "--artifacts", $resolvedArtifacts
        )
        if ($Filter) { $arguments += @("--filter", $Filter) }
        Invoke-Checked -FilePath $executable -ArgumentList $arguments
    }

    if ($selectedBackends.Count -eq 2) {
        Invoke-Checked -FilePath $executable -ArgumentList @(
            "--compare-backends", (Join-Path $resolvedArtifacts "vulkan"), (Join-Path $resolvedArtifacts "opengl"),
            "--artifacts", (Join-Path $resolvedArtifacts "backend-diff")
        )
    }
}
finally {
    Exit-CrownyExclusiveLock -Lock $runtimeLock
    Exit-CrownyOutputLock -Lock $outputReadLock
    Pop-Location
}
