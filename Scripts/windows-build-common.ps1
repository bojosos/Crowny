$ErrorActionPreference = "Stop"

function Invoke-CrownyChecked {
    param(
        [Parameter(Mandatory)] [string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $FilePath $($ArgumentList -join ' ')"
    }
}

function Find-CrownyPremake {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $repositoryPremake = Join-Path $RepositoryRoot "3rdparty\premake\bin\premake5.exe"
    if (Test-Path -LiteralPath $repositoryPremake) { return $repositoryPremake }

    $command = Get-Command "premake5.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    throw "Premake was not found in 3rdparty/premake/bin or PATH. Run Scripts/setup-windows.ps1 first."
}

function Find-CrownyMSBuild {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $vswhere = Join-Path $RepositoryRoot "3rdparty\vswhere\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere was not found at $vswhere. Initialize the repository submodules first."
    }

    $candidate = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
    if (-not $candidate) { throw "Visual Studio 2022 Build Tools with C++ support is required." }
    return $candidate
}

function Find-CrownyMSVCCompiler {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $vswhere = Join-Path $RepositoryRoot "3rdparty\vswhere\vswhere.exe"
    $candidates = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find "VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe")
    $candidate = $candidates | Sort-Object -Descending | Select-Object -First 1
    if (-not $candidate) { throw "The x64 MSVC compiler was not found." }
    return $candidate
}

function Initialize-CrownyBuildEnvironment {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $dependencyRoot = Join-Path $RepositoryRoot ".deps"
    if (-not $env:CROWNY_MONO_ROOT) { $env:CROWNY_MONO_ROOT = Join-Path $env:ProgramFiles "Mono" }
    if (-not $env:VULKAN_SDK) { $env:VULKAN_SDK = Join-Path $dependencyRoot "VulkanSDK" }
    if (-not $env:CROWNY_VMA_INCLUDE) {
        $vulkanInclude = Join-Path $env:VULKAN_SDK "Include"
        if (Test-Path -LiteralPath (Join-Path $vulkanInclude "vma\vk_mem_alloc.h")) {
            $env:CROWNY_VMA_INCLUDE = $vulkanInclude
        }
        else {
            $env:CROWNY_VMA_INCLUDE = Join-Path $dependencyRoot "VulkanSDK\Include"
        }
    }
    if (-not $env:CROWNY_OPENAL_ROOT) { $env:CROWNY_OPENAL_ROOT = Join-Path $dependencyRoot "openal" }
    if (-not $env:CROWNY_PHYSICS_ROOT) { $env:CROWNY_PHYSICS_ROOT = Join-Path $dependencyRoot "physics\install" }
    if (-not $env:CROWNY_SPIRV_CROSS_ROOT) { $env:CROWNY_SPIRV_CROSS_ROOT = Join-Path $dependencyRoot "spirv-cross\install" }
}

function Get-CrownyWorkspaceConfiguration {
    param(
        [Parameter(Mandatory)] [string]$Configuration,
        [Parameter(Mandatory)] [string]$Sanitizer
    )

    if ($Sanitizer -eq "Address") {
        if ($Configuration -eq "Dist") { throw "Dist does not have an AddressSanitizer configuration." }
        return "${Configuration}ASan"
    }
    return $Configuration
}

function Get-CrownyOutputConfiguration {
    param(
        [Parameter(Mandatory)] [string]$Configuration,
        [Parameter(Mandatory)] [string]$Sanitizer
    )

    if ($Sanitizer -eq "Address") { return "$Configuration-address" }
    return $Configuration
}

function Get-CrownyBuildOutputConfigurations {
    param([Parameter(Mandatory)] [string]$WorkspaceConfiguration)

    $configurations = @($WorkspaceConfiguration)
    if ($WorkspaceConfiguration -eq "DebugASan") { $configurations += "Debug" }
    if ($WorkspaceConfiguration -eq "ReleaseASan") { $configurations += "Release" }
    return @($configurations | Sort-Object -Unique)
}

function Get-CrownyHash {
    param(
        [string[]]$Files = @(),
        [string[]]$Values = @()
    )

    $records = [Collections.Generic.List[string]]::new()
    foreach ($file in @($Files | Sort-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $file)) {
            $records.Add("missing|$file")
            continue
        }
        $item = Get-Item -LiteralPath $file
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        $records.Add("file|$($item.FullName)|$hash")
    }
    foreach ($value in $Values) { $records.Add("value|$value") }

    $bytes = [Text.Encoding]::UTF8.GetBytes(($records -join "`n"))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha256.ComputeHash($bytes))).Replace("-", "") }
    finally { $sha256.Dispose() }
}

function Get-CrownyLockName {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Name
    )

    $identity = Get-CrownyHash -Files @() -Values @([IO.Path]::GetFullPath($RepositoryRoot).ToLowerInvariant(), $Name)
    return $identity.Substring(0, 20)
}

function Enter-CrownyExclusiveLock {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Name,
        [switch]$Wait
    )

    $lockRoot = Join-Path $RepositoryRoot ".deps\locks"
    New-Item -ItemType Directory -Force -Path $lockRoot | Out-Null
    $lockPath = Join-Path $lockRoot "$(Get-CrownyLockName -RepositoryRoot $RepositoryRoot -Name $Name).lock"
    $ownerPath = "$lockPath.owner.json"

    $reportedOwner = $false
    while (-not $stream) {
        try {
            $stream = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
        catch [IO.IOException] {
            $owner = ""
            try { $owner = [IO.File]::ReadAllText($ownerPath) } catch { }
            if (-not $owner) { $owner = "owner details unavailable" }
            if (-not $Wait) { throw "A Crowny $Name operation is already running. $owner" }
            if (-not $reportedOwner) {
                Write-Host "Waiting for the Crowny $Name operation. $owner"
                $reportedOwner = $true
            }
            Start-Sleep -Seconds 1
        }
    }

    $ownerRecord = [ordered]@{
        pid = $PID
        command = [Environment]::CommandLine
        startedUtc = [DateTime]::UtcNow.ToString("o")
    } | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($ownerPath, $ownerRecord, [Text.UTF8Encoding]::new($false))

    return [pscustomobject]@{ Stream = $stream; Path = $lockPath; OwnerPath = $ownerPath }
}

function Exit-CrownyExclusiveLock {
    param($Lock)

    if (-not $Lock) { return }
    try { Remove-Item -LiteralPath $Lock.OwnerPath -Force -ErrorAction SilentlyContinue } catch { }
    $Lock.Stream.Dispose()
    try { Remove-Item -LiteralPath $Lock.Path -Force -ErrorAction SilentlyContinue } catch { }
}

function Get-CrownyOutputGatePath {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Configuration
    )

    $lockRoot = Join-Path $RepositoryRoot ".deps\locks"
    New-Item -ItemType Directory -Force -Path $lockRoot | Out-Null
    $identity = Get-CrownyLockName -RepositoryRoot $RepositoryRoot -Name "output-$Configuration"
    $gatePath = Join-Path $lockRoot "$identity.gate"
    if (-not (Test-Path -LiteralPath $gatePath)) {
        try {
            $stream = [IO.File]::Open($gatePath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::ReadWrite)
            $stream.Dispose()
        }
        catch [IO.IOException] { }
    }
    return $gatePath
}

function Get-CrownyOutputLockOwners {
    param([Parameter(Mandatory)] [string]$GatePath)

    $owners = [Collections.Generic.List[string]]::new()
    foreach ($ownerPath in @("$GatePath.writer.owner.json") + @(Get-ChildItem -Path "$GatePath.reader.*.owner.json" -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })) {
        if (-not (Test-Path -LiteralPath $ownerPath)) { continue }
        try {
            $owner = Get-Content -LiteralPath $ownerPath -Raw | ConvertFrom-Json
            if (Get-Process -Id $owner.pid -ErrorAction SilentlyContinue) {
                $owners.Add("PID $($owner.pid): $($owner.command)")
            }
            else {
                Remove-Item -LiteralPath $ownerPath -Force -ErrorAction SilentlyContinue
            }
        }
        catch { }
    }
    if ($owners.Count -eq 0) { return "owner details unavailable" }
    return $owners -join "; "
}

function Enter-CrownyOutputWriteLock {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Configuration,
        [switch]$Wait
    )

    $gatePath = Get-CrownyOutputGatePath -RepositoryRoot $RepositoryRoot -Configuration $Configuration
    $null = Get-CrownyOutputLockOwners -GatePath $gatePath
    $ownerPath = "$gatePath.writer.owner.json"
    $reportedOwner = $false
    while (-not $stream) {
        try {
            $stream = [IO.File]::Open($gatePath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
        catch [IO.IOException] {
            $owners = Get-CrownyOutputLockOwners -GatePath $gatePath
            if (-not $Wait) { throw "Crowny $Configuration outputs are in use. $owners" }
            if (-not $reportedOwner) {
                Write-Host "Waiting to update Crowny $Configuration outputs. $owners"
                $reportedOwner = $true
            }
            Start-Sleep -Seconds 1
        }
    }

    $ownerRecord = [ordered]@{
        pid = $PID
        command = [Environment]::CommandLine
        startedUtc = [DateTime]::UtcNow.ToString("o")
    } | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($ownerPath, $ownerRecord, [Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{ Stream = $stream; OwnerPath = $ownerPath; Configuration = $Configuration }
}

function Enter-CrownyOutputReadLock {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Configuration,
        [switch]$Wait
    )

    $gatePath = Get-CrownyOutputGatePath -RepositoryRoot $RepositoryRoot -Configuration $Configuration
    $null = Get-CrownyOutputLockOwners -GatePath $gatePath
    $ownerPath = "$gatePath.reader.$PID-$([Guid]::NewGuid().ToString('N')).owner.json"
    $reportedOwner = $false
    while (-not $stream) {
        try {
            $stream = [IO.File]::Open($gatePath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        }
        catch [IO.IOException] {
            $owners = Get-CrownyOutputLockOwners -GatePath $gatePath
            if (-not $Wait) { throw "Crowny $Configuration outputs are being updated. $owners" }
            if (-not $reportedOwner) {
                Write-Host "Waiting to test Crowny $Configuration outputs. $owners"
                $reportedOwner = $true
            }
            Start-Sleep -Seconds 1
        }
    }

    $ownerRecord = [ordered]@{
        pid = $PID
        command = [Environment]::CommandLine
        startedUtc = [DateTime]::UtcNow.ToString("o")
    } | ConvertTo-Json -Compress
    [IO.File]::WriteAllText($ownerPath, $ownerRecord, [Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{ Stream = $stream; OwnerPath = $ownerPath; Configuration = $Configuration }
}

function Exit-CrownyOutputLock {
    param($Lock)

    if (-not $Lock) { return }
    try { Remove-Item -LiteralPath $Lock.OwnerPath -Force -ErrorAction SilentlyContinue } catch { }
    $Lock.Stream.Dispose()
}

function Enter-CrownyCompilerLease {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [ValidateRange(0, 64)] [int]$RequestedJobs = 0
    )

    $processorBudget = [Math]::Max(1, [Environment]::ProcessorCount)
    $reservedForSecondBuild = [Math]::Min(4, [Math]::Floor($processorBudget / 3))
    $automaticJobs = [Math]::Max(1, $processorBudget - $reservedForSecondBuild)
    $wantedJobs = if ($RequestedJobs -eq 0) { $automaticJobs } else { [Math]::Min($RequestedJobs, $processorBudget) }
    # Automatic builds may accept the reserve left by a larger build.
    # An explicit -Jobs value is a fixed reservation: agents asking for all cores
    # wait for that capacity instead of silently receiving a smaller build.
    $minimumJobs = if ($RequestedJobs -eq 0) {
        [Math]::Max(1, [Math]::Min($reservedForSecondBuild, $wantedJobs))
    }
    else {
        $wantedJobs
    }
    $leaseRoot = Join-Path $RepositoryRoot ".deps\compiler-leases"
    New-Item -ItemType Directory -Force -Path $leaseRoot | Out-Null
    $reported = $false

    while ($true) {
        $schedulerLock = Enter-CrownyExclusiveLock -RepositoryRoot $RepositoryRoot -Name "compiler-scheduler" -Wait
        try {
            $active = [Collections.Generic.List[object]]::new()
            foreach ($leasePath in @(Get-ChildItem -LiteralPath $leaseRoot -Filter "*.json" -File -ErrorAction SilentlyContinue)) {
                try {
                    $record = Get-Content -LiteralPath $leasePath.FullName -Raw | ConvertFrom-Json
                    if (Get-Process -Id $record.pid -ErrorAction SilentlyContinue) {
                        $active.Add([pscustomobject]@{ Path = $leasePath.FullName; Record = $record })
                    }
                    else {
                        Remove-Item -LiteralPath $leasePath.FullName -Force -ErrorAction SilentlyContinue
                    }
                }
                catch { Remove-Item -LiteralPath $leasePath.FullName -Force -ErrorAction SilentlyContinue }
            }

            $usedJobs = 0
            foreach ($lease in $active) { $usedJobs += [int]$lease.Record.jobs }
            $availableJobs = [Math]::Max(0, $processorBudget - $usedJobs)
            if ($availableJobs -ge $minimumJobs) {
                $grantedJobs = [Math]::Min($wantedJobs, $availableJobs)
                $leasePath = Join-Path $leaseRoot "$PID-$([Guid]::NewGuid().ToString('N')).json"
                [ordered]@{
                    pid = $PID
                    jobs = $grantedJobs
                    command = [Environment]::CommandLine
                    startedUtc = [DateTime]::UtcNow.ToString("o")
                } | ConvertTo-Json | Set-Content -LiteralPath $leasePath -Encoding UTF8
                return [pscustomobject]@{ Path = $leasePath; Jobs = $grantedJobs; RequestedJobs = $RequestedJobs; Budget = $processorBudget }
            }

            if (-not $reported) {
                $owners = @($active | ForEach-Object { "PID $($_.Record.pid) using $($_.Record.jobs) worker(s)" }) -join "; "
                Write-Host "Waiting for compiler capacity ($usedJobs/$processorBudget workers active). $owners"
                $reported = $true
            }
        }
        finally { Exit-CrownyExclusiveLock -Lock $schedulerLock }
        Start-Sleep -Seconds 1
    }
}

function Exit-CrownyCompilerLease {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        $Lease
    )

    if (-not $Lease) { return }
    $schedulerLock = Enter-CrownyExclusiveLock -RepositoryRoot $RepositoryRoot -Name "compiler-scheduler" -Wait
    try { Remove-Item -LiteralPath $Lease.Path -Force -ErrorAction SilentlyContinue }
    finally { Exit-CrownyExclusiveLock -Lock $schedulerLock }
}

function Enter-CrownyProjectReadLock {
    param([Parameter(Mandatory)] [string]$RepositoryRoot, [switch]$Wait)

    $lockRoot = Join-Path $RepositoryRoot ".deps\locks"
    New-Item -ItemType Directory -Force -Path $lockRoot | Out-Null
    $gatePath = Join-Path $lockRoot "projects.gate"
    if (-not (Test-Path -LiteralPath $gatePath)) {
        [IO.File]::WriteAllText($gatePath, "Crowny project generation gate")
    }
    $reported = $false
    while ($true) {
        try {
            return [IO.File]::Open($gatePath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        }
        catch [IO.IOException] {
            if (-not $Wait) { throw "Crowny project generation is currently running. Retry the build after it completes." }
            if (-not $reported) {
                Write-Host "Waiting for Crowny project generation to finish..."
                $reported = $true
            }
            Start-Sleep -Seconds 1
        }
    }
}

function Enter-CrownyProjectWriteLock {
    param([Parameter(Mandatory)] [string]$RepositoryRoot, [switch]$Wait)

    $lockRoot = Join-Path $RepositoryRoot ".deps\locks"
    New-Item -ItemType Directory -Force -Path $lockRoot | Out-Null
    $gatePath = Join-Path $lockRoot "projects.gate"
    if (-not (Test-Path -LiteralPath $gatePath)) {
        [IO.File]::WriteAllText($gatePath, "Crowny project generation gate")
    }
    $reported = $false
    while ($true) {
        try {
            return [IO.File]::Open($gatePath, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
        catch [IO.IOException] {
            if (-not $Wait) { throw "Cannot regenerate projects while a Crowny build is running. Retry after active builds complete." }
            if (-not $reported) {
                Write-Host "Waiting to regenerate projects until active builds finish..."
                $reported = $true
            }
            Start-Sleep -Seconds 1
        }
    }
}

function Get-CrownyProjectFingerprint {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Simd
    )

    $files = [Collections.Generic.List[string]]::new()
    $files.Add((Join-Path $RepositoryRoot "premake5.lua"))
    foreach ($directory in @("Crowny", "Crowny-Editor", "Crowny-Builder", "Crowny-RenderTests", "Crowny-Tests", "Crowny-Sharp", "Crowny-Sandbox")) {
        $files.Add((Join-Path $RepositoryRoot "$directory\premake5.lua"))
    }
    Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot "Scripts") -Filter "premake*.lua" -File | ForEach-Object {
        $files.Add($_.FullName)
    }
    Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot "Crowny\Dependencies") -Filter "premake*.lua" -File -Recurse | ForEach-Object {
        $files.Add($_.FullName)
    }
    $files.Add((Join-Path $RepositoryRoot "3rdparty\premake\premake5.lua"))

    # Premake expands Source/** globs while generating the workspace. Hash the
    # matched file names (not their contents) so adding, removing, or renaming a
    # source invalidates the generated projects without regenerating after every
    # edit.
    $sourceLayout = [Collections.Generic.List[string]]::new()
    foreach ($directory in @("Crowny", "Crowny-Editor", "Crowny-Builder", "Crowny-RenderTests", "Crowny-Tests", "Crowny-Sharp", "Crowny-Sandbox")) {
        $sourceRoot = Join-Path $RepositoryRoot "$directory\Source"
        if (-not (Test-Path -LiteralPath $sourceRoot)) { continue }
        Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object {
            $sourceLayout.Add($_.FullName)
        }
    }

    $generationEnvironment = foreach ($name in @(
        "VULKAN_SDK",
        "CROWNY_VMA_INCLUDE",
        "CROWNY_OPENAL_ROOT",
        "CROWNY_PHYSICS_ROOT",
        "CROWNY_SPIRV_CROSS_ROOT",
        "CROWNY_MONO_ROOT"
    )) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ([string]::IsNullOrWhiteSpace($value)) {
            "$name=<unset>"
            continue
        }

        try { $value = [IO.Path]::GetFullPath($value) }
        catch { }
        "$name=$($value.Replace('\', '/').TrimEnd('/').ToLowerInvariant())"
    }

    # Premake embeds dependency roots into the generated projects. Treat those
    # roots as project inputs so switching between bootstrapped and override
    # SDKs cannot silently reuse stale include and library paths.
    $fingerprintValues = @("vs2022", "with-nodes", $Simd.ToLowerInvariant()) + $generationEnvironment +
                         @($sourceLayout | Sort-Object)
    return Get-CrownyHash -Files $files.ToArray() -Values $fingerprintValues
}

function Ensure-CrownyProjects {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [ValidateSet("SSE4.1", "AVX2")] [string]$Simd = "AVX2",
        [switch]$Force
    )

    $stampRoot = Join-Path $RepositoryRoot ".deps\stamps"
    $stampPath = Join-Path $stampRoot "vs2022-projects.json"
    $solutionPath = Join-Path $RepositoryRoot "Crowny.sln"
    $fingerprint = Get-CrownyProjectFingerprint -RepositoryRoot $RepositoryRoot -Simd $Simd
    $isCurrent = $false
    if (-not $Force -and (Test-Path -LiteralPath $solutionPath) -and (Test-Path -LiteralPath $stampPath)) {
        try {
            $stamp = Get-Content -LiteralPath $stampPath -Raw | ConvertFrom-Json
            $isCurrent = $stamp.fingerprint -eq $fingerprint
        }
        catch { $isCurrent = $false }
    }
    if ($isCurrent) { return }

    $generationLock = Enter-CrownyExclusiveLock -RepositoryRoot $RepositoryRoot -Name "project-generation" -Wait
    $projectGate = $null
    try {
        $projectGate = Enter-CrownyProjectWriteLock -RepositoryRoot $RepositoryRoot -Wait

        $fingerprint = Get-CrownyProjectFingerprint -RepositoryRoot $RepositoryRoot -Simd $Simd
        if (-not $Force -and (Test-Path -LiteralPath $solutionPath) -and (Test-Path -LiteralPath $stampPath)) {
            try {
                $stamp = Get-Content -LiteralPath $stampPath -Raw | ConvertFrom-Json
                if ($stamp.fingerprint -eq $fingerprint) { return }
            }
            catch { }
        }

        Write-Host "Generating the mode-stable Visual Studio 2022 solution..."
        $premake = Find-CrownyPremake -RepositoryRoot $RepositoryRoot
        Push-Location $RepositoryRoot
        try {
            Invoke-CrownyChecked -FilePath $premake -ArgumentList @(
                "vs2022", "--with-nodes", "--simd=$($Simd.ToLowerInvariant())"
            )
        }
        finally { Pop-Location }

        New-Item -ItemType Directory -Force -Path $stampRoot | Out-Null
        [ordered]@{
            fingerprint = $fingerprint
            generatedUtc = [DateTime]::UtcNow.ToString("o")
            command = "premake5 vs2022 --with-nodes --simd=$($Simd.ToLowerInvariant())"
        } | ConvertTo-Json | Set-Content -LiteralPath $stampPath -Encoding UTF8
    }
    finally {
        if ($projectGate) { $projectGate.Dispose() }
        Exit-CrownyExclusiveLock -Lock $generationLock
    }
}

function Get-CrownyManagedFastNoiseSource {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $source = Join-Path $RepositoryRoot "Crowny\Dependencies\FastNoiseLite\CSharp\FastNoiseLite.cs"
    $contents = [IO.File]::ReadAllText($source)
    $unsupported = "private const short OPTIMISE = 512;"
    if (-not $contents.Contains($unsupported)) { return $source }

    $generatedRoot = Join-Path $RepositoryRoot ".deps\generated"
    $generated = Join-Path $generatedRoot "FastNoiseLite.Mono.cs"
    $compatible = $contents.Replace($unsupported, "private const short OPTIMISE = 0;")
    New-Item -ItemType Directory -Force -Path $generatedRoot | Out-Null
    if (-not (Test-Path -LiteralPath $generated) -or [IO.File]::ReadAllText($generated) -cne $compatible) {
        [IO.File]::WriteAllText($generated, $compatible, [Text.UTF8Encoding]::new($false))
    }
    return $generated
}

function Build-CrownyManagedAssemblies {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Configuration
    )

    $managedLock = Enter-CrownyExclusiveLock -RepositoryRoot $RepositoryRoot -Name "managed-assemblies" -Wait
    try {
        $monoRoot = $env:CROWNY_MONO_ROOT
        $mcs = Join-Path $monoRoot "bin\mcs.bat"
        if (-not (Test-Path -LiteralPath $mcs)) { throw "Mono C# compiler was not found: $mcs" }
        $monoExecutable = Join-Path $monoRoot "bin\mono.exe"
        $mcsExecutable = Join-Path $monoRoot "lib\mono\4.5\mcs.exe"

        $engineSources = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot "Crowny-Sharp\Source") -Filter "*.cs" -File -Recurse |
            ForEach-Object { $_.FullName })
        $engineSources += Get-CrownyManagedFastNoiseSource -RepositoryRoot $RepositoryRoot
        $gameSources = @(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot "Crowny-Sandbox\Source") -Filter "*.cs" -File -Recurse |
            ForEach-Object { $_.FullName })

        # Managed assemblies are build products. Keep them under the ignored dependency cache so
        # compiling the editor or process-isolated tests never rewrites committed fallback binaries.
        $managedOutputRoot = Join-Path $RepositoryRoot ".deps\generated\managed\$Configuration"
        $engineAssembly = Join-Path $managedOutputRoot "CrownySharp.dll"
        $gameAssembly = Join-Path $managedOutputRoot "GameAssembly.dll"
        $managedDefine = switch ($Configuration) {
            "Debug" { "CW_DEBUG" }
            "Release" { "CW_RELEASE" }
            "Dist" { "CW_DIST" }
            default { throw "Unsupported managed configuration: $Configuration" }
        }
        $emitDebugSymbols = $Configuration -eq "Debug"
        $debugArgument = if ($emitDebugSymbols) { "-debug+" } else { "-debug-" }
        $optimizeArgument = if ($emitDebugSymbols) { "-optimize-" } else { "-optimize+" }
        $outputs = @($engineAssembly, $gameAssembly)
        if ($emitDebugSymbols) { $outputs += @("$engineAssembly.mdb", "$gameAssembly.mdb") }
        $releaseStaleSymbols = @("$engineAssembly.mdb", "$gameAssembly.mdb", "$engineAssembly.pdb", "$gameAssembly.pdb")
        if (-not $emitDebugSymbols) {
            foreach ($staleSymbols in $releaseStaleSymbols) {
                if (Test-Path -LiteralPath $staleSymbols) { Remove-Item -LiteralPath $staleSymbols -Force }
            }
        }
        $env:CROWNY_MANAGED_ASSEMBLY_ROOT = $managedOutputRoot
        $fingerprint = Get-CrownyHash -Files (@($mcs, $monoExecutable, $mcsExecutable) + $engineSources + $gameSources) -Values @(
            "configuration=$Configuration", "defines=CROWNY_MONO,$managedDefine", "langversion=7.2", "unsafe=true",
            "debug=$emitDebugSymbols", "optimize=$(-not $emitDebugSymbols)"
        )
        $stampRoot = Join-Path $RepositoryRoot ".deps\stamps"
        $stampPath = Join-Path $stampRoot "managed-$($Configuration.ToLowerInvariant()).json"
        if ((Test-Path -LiteralPath $stampPath) -and -not (@($outputs | Where-Object { -not (Test-Path -LiteralPath $_) }).Count)) {
            try {
                $stamp = Get-Content -LiteralPath $stampPath -Raw | ConvertFrom-Json
                if ($stamp.fingerprint -eq $fingerprint) {
                    Write-Host "Managed assemblies are current."
                    return
                }
            }
            catch { }
        }

        New-Item -ItemType Directory -Force -Path $managedOutputRoot | Out-Null
        Write-Host "Building CrownySharp.dll..."
        Invoke-CrownyChecked -FilePath $mcs -ArgumentList (@(
            $debugArgument, $optimizeArgument, "-langversion:7.2", "-unsafe", "-define:CROWNY_MONO,$managedDefine",
            "-target:library", "-out:$engineAssembly"
        ) + $engineSources)
        Write-Host "Building GameAssembly.dll..."
        Invoke-CrownyChecked -FilePath $mcs -ArgumentList (@(
            $debugArgument, $optimizeArgument, "-langversion:7.2", "-define:$managedDefine", "-target:library",
            "-lib:$(Split-Path -Parent $engineAssembly)", "-reference:CrownySharp.dll", "-out:$gameAssembly"
        ) + $gameSources)
        if (-not $emitDebugSymbols) {
            foreach ($staleSymbols in $releaseStaleSymbols) {
                if (Test-Path -LiteralPath $staleSymbols) { Remove-Item -LiteralPath $staleSymbols -Force }
            }
        }
        New-Item -ItemType Directory -Force -Path $stampRoot | Out-Null
        [ordered]@{ fingerprint = $fingerprint; builtUtc = [DateTime]::UtcNow.ToString("o") } |
            ConvertTo-Json | Set-Content -LiteralPath $stampPath -Encoding UTF8
    }
    finally { Exit-CrownyExclusiveLock -Lock $managedLock }
}

function Test-CrownyBuiltinsNeedCooking {
    param([Parameter(Mandatory)] [string]$RepositoryRoot)

    $resourceRoot = Join-Path $RepositoryRoot "Crowny-Editor\Resources"
    $requiredSources = @(
        "Icons\Play.png", "Icons\Pause.png", "Icons\Stop.png", "Icons\File.png", "Icons\Folder.png",
        "Icons\ArrowPointerIcon.png", "Icons\ArrowsIcon.png", "Icons\RotateIcon.png", "Icons\MaximizeIcon.png",
        "Icons\GlobeIcon.png", "Icons\SearchIcon.png", "Icons\ConsoleInfo.png", "Icons\ConsoleWarn.png",
        "Icons\ConsoleError.png", "Icons\AlignLeft.png", "Icons\AlignCenter.png", "Icons\AlignRight.png",
        "Fonts\Roboto\roboto-thin.ttf"
    )
    $sources = @($requiredSources | ForEach-Object { Get-Item -LiteralPath (Join-Path $resourceRoot $_) })
    $sources += @(Get-ChildItem -LiteralPath (Join-Path $resourceRoot "Shaders") -Filter "*.glsl" -File -ErrorAction SilentlyContinue |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw).Contains("#lang") })
    foreach ($source in $sources) {
        $asset = if ($source.Extension -eq ".ttf") { "$($source.FullName).asset" } else { [IO.Path]::ChangeExtension($source.FullName, ".asset") }
        if (-not (Test-Path -LiteralPath $asset)) { return $true }
        if ($source.LastWriteTimeUtc -gt (Get-Item -LiteralPath $asset).LastWriteTimeUtc) { return $true }
    }
    return $false
}

function Update-CrownyEditorResources {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Configuration,
        [Parameter(Mandatory)] [string]$Sanitizer
    )

    $outputConfiguration = Get-CrownyOutputConfiguration -Configuration $Configuration -Sanitizer $Sanitizer
    $editorOutput = Join-Path $RepositoryRoot "bin\$outputConfiguration-windows-x86_64\Crowny-Editor"
    $editorExecutable = Join-Path $editorOutput "Crowny-Editor.exe"
    if (-not (Test-Path -LiteralPath $editorExecutable)) { throw "Editor executable was not found: $editorExecutable" }

    if (Test-CrownyBuiltinsNeedCooking -RepositoryRoot $RepositoryRoot) {
        Write-Host "Cooking changed editor built-ins..."
        $originalAsanOptions = $env:ASAN_OPTIONS
        try {
            if ($Sanitizer -eq "Address") {
                $env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:strict_string_checks=1"
            }
            Push-Location (Join-Path $RepositoryRoot "Crowny-Editor")
            try { Invoke-CrownyChecked -FilePath $editorExecutable -ArgumentList @("--cook-builtins") }
            finally { Pop-Location }
        }
        finally { $env:ASAN_OPTIONS = $originalAsanOptions }
    }
    else {
        Write-Host "Cooked editor built-ins are current."
    }

    Invoke-CrownyChecked -FilePath "powershell" -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $RepositoryRoot "Scripts\pack-builtins.ps1"),
        "-RepositoryRoot", $RepositoryRoot, "-Configuration", $Configuration
    )
    $editorResourceOutput = Join-Path $editorOutput "Resources"
    New-Item -ItemType Directory -Force -Path $editorResourceOutput | Out-Null
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot "Crowny-Editor\Resources\Builtin.cwpack") -Destination $editorResourceOutput -Force
}
