[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "Dist")]
    [string]$Configuration = "Release",
    [ValidateSet("All", "Vulkan", "OpenGL")]
    [string]$Backend = "All",
    [string]$Filter = "",
    [string]$ReferenceRoot = "Crowny-RenderTests/References",
    [string]$ArtifactRoot = "artifacts/render-tests",
    [switch]$UpdateReferences,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot

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

function Find-MSBuild {
    $vswhere = Join-Path $repositoryRoot "3rdparty\vswhere\vswhere.exe"
    $candidate = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
    if (-not $candidate) { throw "Visual Studio 2022 Build Tools with C++ support is required." }
    return $candidate
}

Push-Location $repositoryRoot
try {
    if (-not $NoBuild) {
        if (-not (Test-Path -LiteralPath "Crowny-RenderTests\Crowny-RenderTests.vcxproj")) {
            Invoke-Checked -FilePath (Join-Path $PSScriptRoot "genprojects.bat") -ArgumentList @()
        }
        $msbuild = Find-MSBuild
        Invoke-Checked -FilePath $msbuild -ArgumentList @(
            "Crowny-RenderTests\Crowny-RenderTests.vcxproj", "/nologo", "/v:minimal", "/m:1", "/nodeReuse:false",
            "/p:BuildInParallel=false", "/p:MultiProcessorCompilation=false", "/p:UseMultiToolTask=false",
            "/p:Configuration=$Configuration Win64", "/p:Platform=x64"
        )
    }

    $executable = Join-Path $repositoryRoot "bin\$Configuration-windows-x86_64\Crowny-RenderTests\Crowny-RenderTests.exe"
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
    Pop-Location
}
