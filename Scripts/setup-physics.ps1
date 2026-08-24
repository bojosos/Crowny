[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [ValidateSet("SSE4.1", "AVX2")]
    [string]$Simd = "AVX2",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$physicsRoot = Join-Path $repositoryRoot ".deps\physics"
$buildRoot = Join-Path $physicsRoot "build"
$installRoot = Join-Path $physicsRoot "install\$Configuration"
$simdLevel = $Simd.ToLowerInvariant()
$simdCMakeOptions = if ($Simd -eq "AVX2") {
    @("-DCMAKE_C_FLAGS=/arch:AVX2", "-DCMAKE_CXX_FLAGS=/arch:AVX2")
} else {
    @()
}

$dependencies = @(
    @{ Name = "box3d"; Repository = "https://github.com/erincatto/box3d.git"; Commit = "8441b4a06d6d09dcfb0b0f704df4d847d1437b92"; Required = "include\box3d\box3d.h" },
    @{ Name = "jolt"; Repository = "https://github.com/jrouwe/JoltPhysics.git"; Commit = "e77f175595e64cb44218cc9d9d56fc365ad0e36a"; Required = "Jolt\Jolt.h" },
    @{ Name = "bullet3"; Repository = "https://github.com/bulletphysics/bullet3.git"; Commit = "2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5"; Required = "src\btBulletDynamicsCommon.h" }
)

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

function Find-CMake {
    $command = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        (Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "CMake 3.22 or newer is required."
}

function Assert-ChildPath {
    param([Parameter(Mandatory)] [string]$Path)

    $root = [IO.Path]::GetFullPath($physicsRoot).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath($Path)
    if (-not $candidate.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside $physicsRoot`: $candidate"
    }
}

function Get-GitCommit {
    param([Parameter(Mandatory)] [string]$Path)

    if (-not (Test-Path -LiteralPath (Join-Path $Path ".git"))) { return "" }
    $commit = & git -C $Path rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0) { return "" }
    return $commit.Trim()
}

function Initialize-Dependency {
    param([Parameter(Mandatory)] [hashtable]$Dependency)

    $target = Join-Path $physicsRoot $Dependency.Name
    $requiredFile = Join-Path $target $Dependency.Required
    $valid = (Get-GitCommit -Path $target) -eq $Dependency.Commit -and
        (Test-Path -LiteralPath $requiredFile)
    if ($valid) { return $target }

    $staging = Join-Path $physicsRoot ("staging-" + $Dependency.Name)
    Assert-ChildPath -Path $target
    Assert-ChildPath -Path $staging
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }

    Write-Host "Fetching $($Dependency.Name) at $($Dependency.Commit)..."
    Invoke-Checked -FilePath "git" -ArgumentList @(
        "clone", "--filter=blob:none", "--no-checkout", $Dependency.Repository, $staging
    )
    Invoke-Checked -FilePath "git" -ArgumentList @(
        "-C", $staging, "fetch", "--depth", "1", "origin", $Dependency.Commit
    )
    Invoke-Checked -FilePath "git" -ArgumentList @(
        "-C", $staging, "checkout", "--detach", $Dependency.Commit
    )
    if ((Get-GitCommit -Path $staging) -ne $Dependency.Commit -or
        -not (Test-Path -LiteralPath (Join-Path $staging $Dependency.Required))) {
        throw "Dependency validation failed for $($Dependency.Name)."
    }

    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    Move-Item -LiteralPath $staging -Destination $target
    return $target
}

function Invoke-CMakeBuild {
    param(
        [Parameter(Mandatory)] [string]$Source,
        [Parameter(Mandatory)] [string]$Build,
        [Parameter(Mandatory)] [string[]]$Options
    )

    New-Item -ItemType Directory -Force -Path $Build | Out-Null
    Invoke-Checked -FilePath $script:cmake -ArgumentList (@(
        "-S", $Source, "-B", $Build, "-A", "x64"
    ) + $script:simdCMakeOptions + $Options)
    Invoke-Checked -FilePath $script:cmake -ArgumentList @(
        "--build", $Build, "--config", $Configuration, "--parallel"
    )
}

if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) { throw "Git is required." }
$cmake = Find-CMake

New-Item -ItemType Directory -Force -Path $physicsRoot, $buildRoot, $installRoot | Out-Null
$stamp = Join-Path $installRoot ".crowny-physics-version"
$expectedStamp = ($dependencies | ForEach-Object { "$($_.Name)=$($_.Commit)" }) -join "`n"
$expectedStamp += "`nsimd=$simdLevel-v1"
$requiredLibraries = @(
    (Join-Path $installRoot "lib\box3d.lib"),
    (Join-Path $installRoot "lib\Jolt.lib"),
    (Join-Path $installRoot "lib\BulletDynamics.lib"),
    (Join-Path $installRoot "lib\BulletCollision.lib"),
    (Join-Path $installRoot "lib\LinearMath.lib")
)
if (-not $Force -and (Test-Path -LiteralPath $stamp) -and
    (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $expectedStamp.Trim() -and
    -not ($requiredLibraries | Where-Object { -not (Test-Path -LiteralPath $_) })) {
    Write-Host "Physics dependencies are already built for $Configuration."
    return
}

$sources = @{}
foreach ($dependency in $dependencies) {
    $sources[$dependency.Name] = Initialize-Dependency -Dependency $dependency
}

# Box3D 0.1.0 hard-codes the static MSVC CRT. This local, verified CMake edit keeps
# Crowny and Box3D on the same DLL runtime without maintaining a source fork.
$boxCMake = Join-Path $sources.box3d "CMakeLists.txt"
$boxCMakeText = Get-Content -LiteralPath $boxCMake -Raw
$boxStaticRuntime = 'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")'
$boxDynamicRuntime = 'set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")'
if ($boxCMakeText.Contains($boxStaticRuntime)) {
    Set-Content -LiteralPath $boxCMake -Value ($boxCMakeText.Replace($boxStaticRuntime, $boxDynamicRuntime)) -NoNewline
} elseif (-not $boxCMakeText.Contains($boxDynamicRuntime)) {
    throw "Box3D's runtime setting changed upstream; update setup-physics.ps1 before building."
}

$boxBuild = Join-Path $buildRoot "box3d"
Invoke-CMakeBuild -Source $sources.box3d -Build $boxBuild -Options @(
    "-DBOX3D_SAMPLES=OFF", "-DBOX3D_UNIT_TESTS=OFF", "-DBOX3D_BENCHMARKS=OFF",
    "-DBOX3D_DOCS=OFF", "-DBOX3D_VALIDATE=OFF", "-DBUILD_SHARED_LIBS=OFF"
)
New-Item -ItemType Directory -Force -Path (Join-Path $installRoot "include\box3d"), (Join-Path $installRoot "lib") | Out-Null
Copy-Item -Path (Join-Path $sources.box3d "include\box3d\*") -Destination (Join-Path $installRoot "include\box3d") -Recurse -Force
$boxLibrary = Get-ChildItem -LiteralPath $boxBuild -Filter "box3d*.lib" -File -Recurse |
    Where-Object { $_.FullName -match [regex]::Escape("\$Configuration\") } | Select-Object -First 1
if (-not $boxLibrary) { throw "The Box3D library was not produced." }
Copy-Item -LiteralPath $boxLibrary.FullName -Destination (Join-Path $installRoot "lib\box3d.lib") -Force

$joltBuild = Join-Path $buildRoot "jolt"
$useAvx2 = if ($Simd -eq "AVX2") { "ON" } else { "OFF" }
$useSse41 = if ($Simd -eq "SSE4.1") { "ON" } else { "OFF" }
Invoke-CMakeBuild -Source (Join-Path $sources.jolt "Build") -Build $joltBuild -Options @(
    "-DCMAKE_INSTALL_PREFIX=$installRoot", "-DUSE_STATIC_MSVC_RUNTIME_LIBRARY=OFF",
    "-DINTERPROCEDURAL_OPTIMIZATION=OFF", "-DENABLE_ALL_WARNINGS=OFF", "-DENABLE_OBJECT_STREAM=OFF",
    "-DDEBUG_RENDERER_IN_DEBUG_AND_RELEASE=OFF", "-DPROFILER_IN_DEBUG_AND_RELEASE=OFF",
    "-DFLOATING_POINT_EXCEPTIONS_ENABLED=OFF",
    "-DUSE_SSE4_1=$useSse41", "-DUSE_SSE4_2=OFF", "-DUSE_AVX=OFF", "-DUSE_AVX2=$useAvx2", "-DUSE_AVX512=OFF",
    "-DUSE_LZCNT=OFF", "-DUSE_TZCNT=OFF", "-DUSE_F16C=OFF", "-DUSE_FMADD=OFF",
    "-DJPH_USE_DX12=OFF", "-DJPH_USE_VK=OFF", "-DJPH_USE_MTL=OFF", "-DJPH_USE_CPU_COMPUTE=OFF",
    "-DTARGET_UNIT_TESTS=OFF", "-DTARGET_HELLO_WORLD=OFF", "-DTARGET_PERFORMANCE_TEST=OFF",
    "-DTARGET_SAMPLES=OFF", "-DTARGET_VIEWER=OFF"
)
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "--install", $joltBuild, "--config", $Configuration, "--prefix", $installRoot
)

$bulletBuild = Join-Path $buildRoot "bullet3"
Invoke-CMakeBuild -Source $sources.bullet3 -Build $bulletBuild -Options @(
    "-DCMAKE_INSTALL_PREFIX=$installRoot", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DUSE_MSVC_RUNTIME_LIBRARY_DLL=ON",
    "-DBUILD_SHARED_LIBS=OFF", "-DBUILD_BULLET2_DEMOS=OFF", "-DBUILD_CPU_DEMOS=OFF",
    "-DBUILD_OPENGL3_DEMOS=OFF", "-DBUILD_EXTRAS=OFF", "-DBUILD_UNIT_TESTS=OFF",
    "-DBUILD_PYBULLET=OFF", "-DINSTALL_LIBS=ON"
)
Invoke-Checked -FilePath $cmake -ArgumentList @(
    "--install", $bulletBuild, "--config", $Configuration, "--prefix", $installRoot
)

if ($Configuration -eq "Debug") {
    foreach ($libraryName in @("BulletDynamics", "BulletCollision", "LinearMath")) {
        $debugLibrary = Join-Path $installRoot "lib\${libraryName}_Debug.lib"
        $canonicalLibrary = Join-Path $installRoot "lib\$libraryName.lib"
        if (-not (Test-Path -LiteralPath $debugLibrary)) {
            throw "Bullet did not install its Debug library: $debugLibrary"
        }
        Copy-Item -LiteralPath $debugLibrary -Destination $canonicalLibrary -Force
    }
}

$missingLibraries = $requiredLibraries | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingLibraries) { throw "Missing physics libraries after build: $($missingLibraries -join ', ')" }
Set-Content -LiteralPath $stamp -Value $expectedStamp
Write-Host "Physics dependencies are ready in $installRoot."
