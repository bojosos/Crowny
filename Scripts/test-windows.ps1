[CmdletBinding()]
param(
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
    [string]$Filter = "",
    [switch]$ProcessIsolated,
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "windows-build-common.ps1")

Initialize-CrownyBuildEnvironment -RepositoryRoot $repositoryRoot
$buildArguments = @{
    Target = "Tests"
    Configuration = $Configuration
    Sanitizer = $Sanitizer
    Jobs = $Jobs
    Simd = $Simd
    CompilerCache = $CompilerCache
}
if ($Clean) { $buildArguments.Clean = $true }
if ($Profile) { $buildArguments.Profile = $true }
& (Join-Path $PSScriptRoot "build-windows.ps1") @buildArguments

if ($ProcessIsolated) {
    Build-CrownyManagedAssemblies -RepositoryRoot $repositoryRoot -Configuration $Configuration
}

$workspaceConfiguration = Get-CrownyWorkspaceConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
$outputReadLock = Enter-CrownyOutputReadLock -RepositoryRoot $repositoryRoot -Configuration $workspaceConfiguration -Wait
try {
    $outputConfiguration = Get-CrownyOutputConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
    $testOutput = Join-Path $repositoryRoot "bin\$outputConfiguration-windows-x86_64\Crowny-Tests"
    $testExecutable = Join-Path $testOutput "Crowny-Tests.exe"
    if (-not (Test-Path -LiteralPath $testExecutable)) { throw "Test executable was not found: $testExecutable" }

    $originalPath = $env:PATH
    $originalAsanOptions = $env:ASAN_OPTIONS
    try {
        $env:PATH = "$($env:VULKAN_SDK)\Bin;$($env:CROWNY_MONO_ROOT)\bin;$testOutput;$originalPath"
        if ($Sanitizer -eq "Address") {
            $env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
        }

        Push-Location $repositoryRoot
        try {
            if ($ProcessIsolated) {
                Write-Host "Running the hidden process-isolated test lane..."
                $isolatedTests = @(& $testExecutable --list-tests "[.ProcessIsolated]~[Benchmark]" --verbosity quiet)
                if ($LASTEXITCODE -ne 0) {
                    throw "Failed to enumerate the hidden process-isolated test lane."
                }
                foreach ($isolatedTest in $isolatedTests) {
                    if ([string]::IsNullOrWhiteSpace($isolatedTest)) { continue }
                    Write-Host "Running isolated test: $isolatedTest"
                    Invoke-CrownyChecked -FilePath $testExecutable -ArgumentList @($isolatedTest)
                }
            }

            $testArguments = @()
            if ($Filter) { $testArguments += $Filter }
            Write-Host "Running Crowny-Tests from the repository root..."
            Invoke-CrownyChecked -FilePath $testExecutable -ArgumentList $testArguments
        }
        finally { Pop-Location }
    }
    finally {
        $env:PATH = $originalPath
        $env:ASAN_OPTIONS = $originalAsanOptions
    }
}
finally {
    Exit-CrownyOutputLock -Lock $outputReadLock
}
