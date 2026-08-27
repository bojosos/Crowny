[CmdletBinding()]
param(
    [ValidateSet("Clean", "NoOp", "TouchedSource")]
    [string[]]$Scenario = @("Clean", "NoOp", "TouchedSource"),
    [ValidateSet("Engine", "Editor", "Tests", "RenderTests", "All")]
    [string]$Target = "Tests",
    [ValidateSet("Debug", "Release", "Dist")]
    [string]$Configuration = "Release",
    [ValidateSet("None", "Address")]
    [string]$Sanitizer = "None",
    [ValidateRange(0, 64)]
    [int]$Jobs = 0,
    [ValidateSet("None", "Sccache")]
    [string]$CompilerCache = "None",
    [string]$SourceFile = "",
    [switch]$BuildInsights,
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")
$metricsRoot = Join-Path $repositoryRoot "artifacts\build-metrics"
$measurementRoot = Join-Path $metricsRoot "$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))-measurement"
New-Item -ItemType Directory -Force -Path $measurementRoot | Out-Null

$representativeSources = @{
    Engine = "Crowny\Source\Platform\Windows\WindowsPlatformUtils.cpp"
    Editor = "Crowny-Editor\Source\UI\UIUtils.cpp"
    Tests = "Crowny-Tests\Source\StringUtilsTests.cpp"
    RenderTests = "Crowny-RenderTests\Source\RenderTestRunner.cpp"
    All = "Crowny\Source\Platform\Windows\WindowsPlatformUtils.cpp"
}
if (-not $SourceFile) { $SourceFile = $representativeSources[$Target] }
$resolvedSource = [IO.Path]::GetFullPath((Join-Path $repositoryRoot $SourceFile))
if (-not (Test-Path -LiteralPath $resolvedSource)) { throw "Representative source file was not found: $resolvedSource" }
if (-not $resolvedSource.StartsWith(([IO.Path]::GetFullPath($repositoryRoot).TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) {
    throw "The touched source must be inside the repository: $resolvedSource"
}

$vcperf = $null
$vcperfSession = "CrownyBuild-$PID"
$tracePath = Join-Path $measurementRoot "build-insights.etl"
if ($BuildInsights) {
    $vcperfCommand = Get-Command "vcperf.exe" -ErrorAction SilentlyContinue
    if ($vcperfCommand) { $vcperf = $vcperfCommand.Source }
    if (-not $vcperf) {
        $vswhere = Join-Path $repositoryRoot "3rdparty\vswhere\vswhere.exe"
        $installationPath = & $vswhere -latest -products * -property installationPath | Select-Object -First 1
        if ($installationPath) {
            $candidate = Join-Path $installationPath "Common7\IDE\VC\vcperf.exe"
            if (Test-Path -LiteralPath $candidate) { $vcperf = $candidate }
        }
    }
    if (-not $vcperf) { throw "C++ Build Insights (vcperf.exe) is not installed with Visual Studio." }
}

function Invoke-MeasurementBuild {
    param([Parameter(Mandatory)] [string]$ScenarioName, [switch]$CleanBuild)

    $arguments = @{
        Target = $Target
        Configuration = $Configuration
        Sanitizer = $Sanitizer
        Jobs = $Jobs
        Simd = $Simd
        CompilerCache = $CompilerCache
        Profile = $true
    }
    if ($CleanBuild) { $arguments.Clean = $true }

    $startedUtc = [DateTime]::UtcNow
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & (Join-Path $PSScriptRoot "build-windows.ps1") @arguments
    $timer.Stop()

    $profile = Get-ChildItem -LiteralPath $metricsRoot -Filter "metrics.json" -File -Recurse |
        Where-Object {
            $_.LastWriteTimeUtc -ge $startedUtc.AddSeconds(-2) -and
            $_.Directory.Name -like "*-$($Target.ToLowerInvariant())-$($workspaceConfiguration.ToLowerInvariant())"
        } |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $profile) { throw "The $ScenarioName build did not produce a metrics.json file." }
    $profileData = Get-Content -LiteralPath $profile.FullName -Raw | ConvertFrom-Json
    return [ordered]@{
        scenario = $ScenarioName
        wallSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 3)
        metrics = $profile.FullName
        binlog = $profileData.binlog
        peakCompilerWorkingSetBytes = $profileData.peakCompilerWorkingSetBytes
        phases = $profileData.phases
    }
}

$workspaceConfiguration = Get-CrownyWorkspaceConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
$outputConfigurations = Get-CrownyBuildOutputConfigurations -WorkspaceConfiguration $workspaceConfiguration
$operationLocks = [Collections.Generic.List[object]]::new()
foreach ($outputConfiguration in $outputConfigurations) {
    $operationLocks.Add((Enter-CrownyOutputWriteLock -RepositoryRoot $repositoryRoot -Configuration $outputConfiguration -Wait))
}
$originalOutputLock = $env:CROWNY_OUTPUT_WRITE_LOCK
$env:CROWNY_OUTPUT_WRITE_LOCK = "$PID|$($outputConfigurations -join ',')"
try {
    $results = [Collections.Generic.List[object]]::new()
    $traceStarted = $false
    try {
        if ($BuildInsights) {
            & $vcperf /start $vcperfSession /noadmin
            if ($LASTEXITCODE -ne 0) { throw "vcperf failed to start with exit code $LASTEXITCODE." }
            $traceStarted = $true
        }

        foreach ($scenarioName in $Scenario) {
            switch ($scenarioName) {
                "Clean" {
                    $results.Add((Invoke-MeasurementBuild -ScenarioName "Clean" -CleanBuild))
                }
                "NoOp" {
                    Write-Host "Priming the no-op scenario..."
                    & (Join-Path $PSScriptRoot "build-windows.ps1") -Target $Target -Configuration $Configuration -Sanitizer $Sanitizer -Jobs $Jobs -Simd $Simd -CompilerCache $CompilerCache
                    $results.Add((Invoke-MeasurementBuild -ScenarioName "NoOp"))
                }
                "TouchedSource" {
                    Write-Host "Priming the touched-source scenario..."
                    & (Join-Path $PSScriptRoot "build-windows.ps1") -Target $Target -Configuration $Configuration -Sanitizer $Sanitizer -Jobs $Jobs -Simd $Simd -CompilerCache $CompilerCache

                    $originalWriteTime = (Get-Item -LiteralPath $resolvedSource).LastWriteTimeUtc
                    try {
                        (Get-Item -LiteralPath $resolvedSource).LastWriteTimeUtc = [DateTime]::UtcNow.AddSeconds(2)
                        $results.Add((Invoke-MeasurementBuild -ScenarioName "TouchedSource"))
                    }
                    finally { (Get-Item -LiteralPath $resolvedSource).LastWriteTimeUtc = $originalWriteTime }
                }
            }
        }
    }
    finally {
        if ($traceStarted) {
            & $vcperf /stop $vcperfSession $tracePath /noadmin
            if ($LASTEXITCODE -ne 0) { Write-Warning "vcperf failed to stop cleanly; the trace may be incomplete." }
        }
    }

    $summaryPath = Join-Path $measurementRoot "summary.json"
    [ordered]@{
        target = $Target
        configuration = $Configuration
        sanitizer = $Sanitizer
        jobs = $Jobs
        compilerCache = $CompilerCache
        representativeSource = $resolvedSource
        buildInsightsTrace = if ($BuildInsights) { $tracePath } else { $null }
        results = $results
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "Measurement summary: $summaryPath"
}
finally {
    $env:CROWNY_OUTPUT_WRITE_LOCK = $originalOutputLock
    for ($index = $operationLocks.Count - 1; $index -ge 0; $index--) {
        Exit-CrownyOutputLock -Lock $operationLocks[$index]
    }
}
