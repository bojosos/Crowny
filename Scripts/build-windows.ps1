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
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")

function Invoke-CrownyMSBuild {
    param(
        [Parameter(Mandatory)] [string]$MSBuild,
        [Parameter(Mandatory)] [string[]]$Arguments,
        [switch]$CollectProfile
    )

    if (-not $CollectProfile) {
        & $MSBuild @Arguments
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) { throw "MSBuild failed with exit code $exitCode." }
        return [pscustomobject]@{ ExitCode = 0; PeakCompilerWorkingSetBytes = 0 }
    }

    $buildStarted = [DateTime]::Now.AddSeconds(-1)
    $payload = [pscustomobject]@{ Executable = $MSBuild; BuildArguments = $Arguments }
    $job = Start-Job -ScriptBlock {
        param($BuildPayload)
        $jobArguments = @($BuildPayload.BuildArguments)
        & $BuildPayload.Executable @jobArguments 2>&1
        "__CROWNY_EXIT_CODE__=$LASTEXITCODE"
    } -ArgumentList $payload
    $peakCompilerWorkingSet = 0L
    try {
        while ($job.State -eq "Running" -or $job.State -eq "NotStarted") {
            $compilerWorkingSet = 0L
            Get-Process -Name "cl", "c1xx" -ErrorAction SilentlyContinue | ForEach-Object {
                try {
                    if ($_.StartTime -ge $buildStarted) { $compilerWorkingSet += $_.WorkingSet64 }
                }
                catch { }
            }
            if ($compilerWorkingSet -gt $peakCompilerWorkingSet) { $peakCompilerWorkingSet = $compilerWorkingSet }
            Start-Sleep -Milliseconds 200
        }
        $output = @(Receive-Job -Job $job -Wait)
    }
    finally { Remove-Job -Job $job -Force -ErrorAction SilentlyContinue }

    $exitRecord = $output | Where-Object { "$_" -like "__CROWNY_EXIT_CODE__=*" } | Select-Object -Last 1
    $output | Where-Object { "$_" -notlike "__CROWNY_EXIT_CODE__=*" } | ForEach-Object { Write-Host "$_" }
    if (-not $exitRecord) { throw "MSBuild profile job ended without reporting its exit code." }
    $exitCode = [int](("$exitRecord").Substring("__CROWNY_EXIT_CODE__=".Length))
    if ($exitCode -ne 0) { throw "MSBuild failed with exit code $exitCode." }
    return [pscustomobject]@{ ExitCode = $exitCode; PeakCompilerWorkingSetBytes = $peakCompilerWorkingSet }
}

Initialize-CrownyBuildEnvironment -RepositoryRoot $repositoryRoot
$workspaceConfiguration = Get-CrownyWorkspaceConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
$outputWriteLocks = [Collections.Generic.List[object]]::new()
$compilerLease = $null
$projectReadLock = $null
$originalOutputLock = $env:CROWNY_OUTPUT_WRITE_LOCK
$outputConfigurations = Get-CrownyBuildOutputConfigurations -WorkspaceConfiguration $workspaceConfiguration
$outputLockIdentity = "$PID|$($outputConfigurations -join ',')"
$ownsOutputLock = $originalOutputLock -ne $outputLockIdentity
$metrics = [ordered]@{
    target = $Target
    configuration = $Configuration
    sanitizer = $Sanitizer
    workspaceConfiguration = $workspaceConfiguration
    requestedJobs = $Jobs
    jobs = 0
    compilerCache = $CompilerCache
    clean = [bool]$Clean
    outputLocks = $outputConfigurations
    startedUtc = [DateTime]::UtcNow.ToString("o")
    phases = [ordered]@{}
}
$overallTimer = [Diagnostics.Stopwatch]::StartNew()
$lockTimer = [Diagnostics.Stopwatch]::StartNew()

Push-Location $repositoryRoot
try {
    if ($ownsOutputLock) {
        foreach ($outputConfiguration in $outputConfigurations) {
            $outputWriteLocks.Add((Enter-CrownyOutputWriteLock -RepositoryRoot $repositoryRoot -Configuration $outputConfiguration -Wait))
        }
        $env:CROWNY_OUTPUT_WRITE_LOCK = $outputLockIdentity
    }
    $lockTimer.Stop()
    $metrics.phases.buildLockWaitSeconds = [Math]::Round($lockTimer.Elapsed.TotalSeconds, 3)

    $phaseTimer = [Diagnostics.Stopwatch]::StartNew()
    Ensure-CrownyProjects -RepositoryRoot $repositoryRoot -Simd $Simd
    $phaseTimer.Stop()
    $metrics.phases.projectGenerationSeconds = [Math]::Round($phaseTimer.Elapsed.TotalSeconds, 3)

    $projectReadLock = Enter-CrownyProjectReadLock -RepositoryRoot $repositoryRoot -Wait
    $msbuild = Find-CrownyMSBuild -RepositoryRoot $repositoryRoot
    $buildInput = Join-Path $repositoryRoot "Crowny.sln"
    $solutionTarget = switch ($Target) {
        "Engine" { "Crowny" }
        "Editor" { "Crowny-Editor" }
        "Tests" { "Crowny-Tests" }
        "RenderTests" { "Crowny-RenderTests" }
        "All" { $null }
    }

    $profileRoot = $null
    $binlogPath = $null
    if ($Profile) {
        $profileRoot = Join-Path $repositoryRoot "artifacts\build-metrics\$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))-$($Target.ToLowerInvariant())-$($workspaceConfiguration.ToLowerInvariant())"
        New-Item -ItemType Directory -Force -Path $profileRoot | Out-Null
        $binlogPath = Join-Path $profileRoot "msbuild.binlog"
    }

    $originalRealCompiler = $env:CROWNY_REAL_CL
    $originalCacheBuster = $env:SCCACHE_C_CUSTOM_CACHE_BUSTER
    if ($CompilerCache -eq "Sccache") {
        $probeStamp = Join-Path $repositoryRoot ".deps\stamps\sccache-msbuild.json"
        $sccache = Get-Command "sccache.exe" -ErrorAction SilentlyContinue
        if (-not $sccache -or -not (Test-Path -LiteralPath $probeStamp)) {
            throw "sccache is not enabled. Install it and run Scripts/probe-sccache-windows.ps1 first."
        }
        $probe = Get-Content -LiteralPath $probeStamp -Raw | ConvertFrom-Json
        if (-not $probe.passed) { throw "The recorded sccache MSBuild feasibility probe did not pass." }
        $env:CROWNY_REAL_CL = Find-CrownyMSVCCompiler -RepositoryRoot $repositoryRoot
        $env:SCCACHE_C_CUSTOM_CACHE_BUSTER = "Crowny-$workspaceConfiguration"
    }

    $compilerLeaseTimer = [Diagnostics.Stopwatch]::StartNew()
    $compilerLease = Enter-CrownyCompilerLease -RepositoryRoot $repositoryRoot -RequestedJobs $Jobs
    $compilerLeaseTimer.Stop()
    $effectiveJobs = $compilerLease.Jobs
    $metrics.jobs = $effectiveJobs
    $metrics.phases.compilerLeaseWaitSeconds = [Math]::Round($compilerLeaseTimer.Elapsed.TotalSeconds, 3)

    $msbuildArguments = @(
        $buildInput,
        "/nologo",
        "/v:minimal",
        "/m:1",
        "/nodeReuse:false",
        "/p:UseMultiToolTask=false",
        "/p:CL_MPCount=$effectiveJobs",
        "/p:Configuration=$workspaceConfiguration",
        "/p:Platform=Win64"
    )
    if ($solutionTarget) {
        $targetAction = if ($Clean) { "$solutionTarget`:Rebuild" } else { $solutionTarget }
        $msbuildArguments += "/t:$targetAction"
    }
    elseif ($Clean) {
        $msbuildArguments += "/t:Rebuild"
    }
    if ($Profile) {
        $msbuildArguments += "/bl:$binlogPath"
        $msbuildArguments += "/clp:PerformanceSummary"
    }
    if ($CompilerCache -eq "Sccache") {
        $msbuildArguments += "/p:CLToolExe=sccache-cl.cmd"
        $msbuildArguments += "/p:CLToolPath=$PSScriptRoot"
        $msbuildArguments += "/p:DebugInformationFormat=OldStyle"
    }

    $jobMode = if ($Jobs -eq 0) { "auto" } else { "requested $Jobs" }
    Write-Host "Building $Target ($workspaceConfiguration|Win64) with $effectiveJobs compiler worker(s) ($jobMode, $($compilerLease.Budget) total budget)..."
    $phaseTimer.Restart()
    $buildResult = Invoke-CrownyMSBuild -MSBuild $msbuild -Arguments $msbuildArguments -CollectProfile:$Profile
    $phaseTimer.Stop()
    $metrics.phases.nativeBuildSeconds = [Math]::Round($phaseTimer.Elapsed.TotalSeconds, 3)
    $metrics.peakCompilerWorkingSetBytes = $buildResult.PeakCompilerWorkingSetBytes
    Exit-CrownyCompilerLease -RepositoryRoot $repositoryRoot -Lease $compilerLease
    $compilerLease = $null

    if ($Target -eq "Editor" -or $Target -eq "All") {
        $phaseTimer.Restart()
        Build-CrownyManagedAssemblies -RepositoryRoot $repositoryRoot -Configuration $Configuration
        $phaseTimer.Stop()
        $metrics.phases.managedBuildSeconds = [Math]::Round($phaseTimer.Elapsed.TotalSeconds, 3)

        $phaseTimer.Restart()
        Update-CrownyEditorResources -RepositoryRoot $repositoryRoot -Configuration $Configuration -Sanitizer $Sanitizer
        $phaseTimer.Stop()
        $metrics.phases.editorResourcesSeconds = [Math]::Round($phaseTimer.Elapsed.TotalSeconds, 3)
    }

    $overallTimer.Stop()
    $metrics.totalSeconds = [Math]::Round($overallTimer.Elapsed.TotalSeconds, 3)
    $metrics.completedUtc = [DateTime]::UtcNow.ToString("o")
    $cacheStatsPath = $null
    if ($CompilerCache -eq "Sccache") {
        $cacheStats = (& sccache.exe --show-stats 2>&1) | Out-String
        Write-Host $cacheStats
        if ($Profile) {
            $cacheStatsPath = Join-Path $profileRoot "sccache-stats.txt"
            $cacheStats | Set-Content -LiteralPath $cacheStatsPath -Encoding UTF8
            $metrics.sccacheStats = $cacheStatsPath
        }
    }

    if ($Profile) {
        $metrics.binlog = $binlogPath
        $metricsPath = Join-Path $profileRoot "metrics.json"
        $metrics | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $metricsPath -Encoding UTF8
        Write-Host "Build metrics: $metricsPath"
    }

}
finally {
    if ($compilerLease) { Exit-CrownyCompilerLease -RepositoryRoot $repositoryRoot -Lease $compilerLease }
    if ($CompilerCache -eq "Sccache") {
        $env:CROWNY_REAL_CL = $originalRealCompiler
        $env:SCCACHE_C_CUSTOM_CACHE_BUSTER = $originalCacheBuster
    }
    if ($projectReadLock) { $projectReadLock.Dispose() }
    $env:CROWNY_OUTPUT_WRITE_LOCK = $originalOutputLock
    if ($ownsOutputLock) {
        for ($index = $outputWriteLocks.Count - 1; $index -ge 0; $index--) {
            Exit-CrownyOutputLock -Lock $outputWriteLocks[$index]
        }
    }
    Pop-Location
}
