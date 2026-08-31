[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "windows-build-common.ps1")

function Assert-Equal {
    param(
        [Parameter(Mandatory)] [string]$Expected,
        [Parameter(Mandatory)] [string]$Actual,
        [Parameter(Mandatory)] [string]$Message
    )

    $expectedPath = [IO.Path]::GetFullPath($Expected)
    $actualPath = [IO.Path]::GetFullPath($Actual)
    if (-not $expectedPath.Equals($actualPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Message`nExpected: $expectedPath`nActual:   $actualPath"
    }
}

function Assert-True {
    param(
        [Parameter(Mandatory)] [bool]$Condition,
        [Parameter(Mandatory)] [string]$Message
    )

    if (-not $Condition) { throw $Message }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "Crowny build roots $([Guid]::NewGuid().ToString('N'))"
$originalDependencyRoot = $env:CROWNY_DEPS_ROOT
$originalCoordinationRoot = $env:CROWNY_BUILD_COORDINATION_ROOT

try {
    $env:CROWNY_DEPS_ROOT = $null
    $env:CROWNY_BUILD_COORDINATION_ROOT = $null

    $mainRoot = Join-Path $testRoot "main checkout with spaces"
    $linkedRoot = Join-Path $testRoot "linked worktree with spaces"
    $gitCommonDirectory = Join-Path $mainRoot ".git"
    $linkedGitDirectory = Join-Path $gitCommonDirectory "worktrees\linked"
    New-Item -ItemType Directory -Force -Path $gitCommonDirectory, $linkedRoot, $linkedGitDirectory | Out-Null
    Set-Content -LiteralPath (Join-Path $linkedRoot ".git") -Value "gitdir: $linkedGitDirectory"

    $normal = Get-CrownyBuildRoots -RepositoryRoot $mainRoot -GitCommonDirectory $gitCommonDirectory
    Assert-Equal -Expected $mainRoot -Actual $normal.CommonRepositoryRoot -Message "A normal clone must be its own common checkout."
    Assert-Equal -Expected (Join-Path $mainRoot ".deps") -Actual $normal.DependencyRoot -Message "A normal clone must use its own dependency cache."
    Assert-Equal -Expected (Join-Path $mainRoot ".deps\build-coordination") -Actual $normal.CoordinationRoot -Message "A normal clone must keep coordination state under its own .deps."

    $linked = Get-CrownyBuildRoots -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory
    Assert-Equal -Expected $mainRoot -Actual $linked.CommonRepositoryRoot -Message "A linked worktree must resolve the main checkout."
    Assert-Equal -Expected (Join-Path $mainRoot ".deps") -Actual $linked.DependencyRoot -Message "A linked worktree must default installers to the common dependency cache."
    Assert-Equal -Expected $normal.CoordinationRoot -Actual $linked.CoordinationRoot -Message "All linked worktrees must share one coordination root."

    # Wrapper-created state must not be mistaken for an installed SDK cache.
    New-Item -ItemType Directory -Force -Path `
        (Join-Path $linkedRoot ".deps\locks"), `
        (Join-Path $linkedRoot ".deps\stamps"), `
        (Join-Path $linkedRoot ".deps\compiler-leases") | Out-Null
    $linkedOpenAL = Get-CrownyDependencyPath -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory `
        -RelativePath "openal" -ReadyRelativePath "openal\include\AL\al.h"
    Assert-Equal -Expected (Join-Path $mainRoot ".deps\openal") -Actual $linkedOpenAL `
        -Message "Partial local lock/stamp state must not hide the common OpenAL cache."
    $dotNetName = if ($env:OS -eq "Windows_NT") { "dotnet.exe" } else { "dotnet" }
    $linkedDotNet = Get-CrownyDependencyPath -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory `
        -RelativePath "dotnet" -ReadyRelativePath "dotnet\$dotNetName"
    Assert-Equal -Expected (Join-Path $mainRoot ".deps\dotnet") -Actual $linkedDotNet `
        -Message "Partial local lock/stamp state must not hide the common .NET SDK."

    $localVulkanHeader = Join-Path $linkedRoot ".deps\VulkanSDK\Include\vulkan\vulkan.h"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $localVulkanHeader) | Out-Null
    Set-Content -LiteralPath $localVulkanHeader -Value "test"
    $linkedVulkan = Get-CrownyDependencyPath -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory `
        -RelativePath "VulkanSDK" -ReadyRelativePath "VulkanSDK\Include\vulkan\vulkan.h"
    Assert-Equal -Expected (Join-Path $linkedRoot ".deps\VulkanSDK") -Actual $linkedVulkan `
        -Message "A ready worktree-local dependency component must take precedence."

    $explicitDependencyRoot = Join-Path $testRoot "explicit dependency cache"
    $explicitCoordinationRoot = Join-Path $testRoot "explicit coordination state"
    $parameterDependencyRoot = Join-Path $testRoot "parameter dependency cache"
    $parameterRoots = Get-CrownyBuildRoots -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory `
        -DependencyRoot $parameterDependencyRoot
    Assert-Equal -Expected $parameterDependencyRoot -Actual $parameterRoots.DependencyRoot `
        -Message "The explicit DependencyRoot parameter must override worktree discovery."

    $env:CROWNY_DEPS_ROOT = $explicitDependencyRoot
    $env:CROWNY_BUILD_COORDINATION_ROOT = $explicitCoordinationRoot
    $overridden = Get-CrownyBuildRoots -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory
    Assert-Equal -Expected $explicitDependencyRoot -Actual $overridden.DependencyRoot -Message "CROWNY_DEPS_ROOT must override worktree discovery."
    Assert-Equal -Expected $explicitCoordinationRoot -Actual $overridden.CoordinationRoot -Message "CROWNY_BUILD_COORDINATION_ROOT must override worktree discovery."
    $overriddenOpenAL = Get-CrownyDependencyPath -RepositoryRoot $linkedRoot -GitCommonDirectory $gitCommonDirectory `
        -RelativePath "openal" -ReadyRelativePath "openal\include\AL\al.h"
    Assert-Equal -Expected (Join-Path $explicitDependencyRoot "openal") -Actual $overriddenOpenAL `
        -Message "An explicit dependency root must win even when a local component is ready."

    $env:CROWNY_DEPS_ROOT = $null
    $sharedMainName = Get-CrownyLockName -RepositoryRoot $mainRoot -Name "compiler-scheduler" -Scope Shared
    $sharedLinkedName = Get-CrownyLockName -RepositoryRoot $linkedRoot -Name "compiler-scheduler" -Scope Shared
    Assert-True -Condition ($sharedMainName -eq $sharedLinkedName) -Message "Compiler scheduler identities must be shared across worktrees."
    $worktreeMainName = Get-CrownyLockName -RepositoryRoot $mainRoot -Name "projects"
    $worktreeLinkedName = Get-CrownyLockName -RepositoryRoot $linkedRoot -Name "projects"
    Assert-True -Condition ($worktreeMainName -ne $worktreeLinkedName) -Message "Project/output identities must remain worktree-scoped."

    $mainOutputGate = Get-CrownyOutputGatePath -RepositoryRoot $mainRoot -Configuration "Release"
    $linkedOutputGate = Get-CrownyOutputGatePath -RepositoryRoot $linkedRoot -Configuration "Release"
    Assert-Equal -Expected $explicitCoordinationRoot -Actual (Split-Path -Parent (Split-Path -Parent $mainOutputGate)) `
        -Message "Output locks must live below the shared coordination root."
    Assert-True -Condition ($mainOutputGate -ne $linkedOutputGate) -Message "Independent worktree outputs must not share one gate."

    $mainProjectLock = Enter-CrownyProjectReadLock -RepositoryRoot $mainRoot
    $linkedProjectLock = Enter-CrownyProjectReadLock -RepositoryRoot $linkedRoot
    try {
        Assert-True -Condition ($mainProjectLock.Name -ne $linkedProjectLock.Name) `
            -Message "Independent worktree project files must not share one gate."
    }
    finally {
        $linkedProjectLock.Dispose()
        $mainProjectLock.Dispose()
    }

    $leaseRoot = Join-Path $explicitCoordinationRoot "compiler-leases"
    New-Item -ItemType Directory -Force -Path $leaseRoot | Out-Null
    $deadLease = Join-Path $leaseRoot "dead.json"
    $corruptLease = Join-Path $leaseRoot "corrupt.json"
    $reusedPidLease = Join-Path $leaseRoot "reused-pid.json"
    [ordered]@{ pid = 2147483647; jobs = 64; command = "dead" } | ConvertTo-Json | Set-Content -LiteralPath $deadLease
    Set-Content -LiteralPath $corruptLease -Value "not json"
    [ordered]@{
        pid = $PID
        processStartedUtc = "2000-01-01T00:00:00.0000000Z"
        jobs = 64
        command = "reused PID"
    } | ConvertTo-Json | Set-Content -LiteralPath $reusedPidLease

    $lease = Enter-CrownyCompilerLease -RepositoryRoot $linkedRoot -RequestedJobs 1
    try {
        Assert-True -Condition (-not (Test-Path -LiteralPath $deadLease)) -Message "Dead compiler leases must be removed."
        Assert-True -Condition (-not (Test-Path -LiteralPath $corruptLease)) -Message "Corrupt compiler leases must be removed."
        Assert-True -Condition (-not (Test-Path -LiteralPath $reusedPidLease)) -Message "Leases from a reused PID must be removed."
        Assert-True -Condition (Test-Path -LiteralPath $lease.Path) -Message "The granted compiler lease must be persisted."
        Assert-Equal -Expected $leaseRoot -Actual (Split-Path -Parent $lease.Path) -Message "Compiler leases must use the shared pool."
    }
    finally {
        Exit-CrownyCompilerLease -RepositoryRoot $linkedRoot -Lease $lease
    }
    Assert-True -Condition (-not (Test-Path -LiteralPath $lease.Path)) -Message "Released compiler leases must be removed."

    $spirvSetupPath = Join-Path $PSScriptRoot "setup-spirv-cross.ps1"
    $spirvTokens = $null
    $spirvErrors = $null
    $spirvAst = [Management.Automation.Language.Parser]::ParseFile(
        $spirvSetupPath,
        [ref]$spirvTokens,
        [ref]$spirvErrors)
    Assert-True -Condition ($spirvErrors.Count -eq 0) -Message "setup-spirv-cross.ps1 must parse cleanly."
    $componentRootAssignments = @($spirvAst.FindAll({
        param($node)
        if ($node -isnot [Management.Automation.Language.AssignmentStatementAst]) { return $false }
        if ($node.Left -isnot [Management.Automation.Language.VariableExpressionAst]) { return $false }
        if ($node.Left.VariablePath.UserPath -ne "dependencyRoot") { return $false }
        return $node.Right.Extent.Text -match 'Join-Path\s+\$dependencyCacheRoot\s+["'']spirv-cross["'']'
    }, $true))
    Assert-True -Condition ($componentRootAssignments.Count -eq 1) `
        -Message "SPIRV-Cross setup must derive its component root from the resolved dependency cache."

    Write-Host "windows-build-common diagnostics passed."
}
finally {
    $env:CROWNY_DEPS_ROOT = $originalDependencyRoot
    $env:CROWNY_BUILD_COORDINATION_ROOT = $originalCoordinationRoot
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
