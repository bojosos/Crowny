#!/usr/bin/env bash
set -euo pipefail

configuration="${1:-Release}"
if [[ "$configuration" != "Debug" && "$configuration" != "Release" ]]; then
    echo "usage: $0 [Debug|Release] [avx2|sse4.1]" >&2
    exit 2
fi

simd="${2:-avx2}"
if [[ "$simd" != "avx2" && "$simd" != "sse4.1" ]]; then
    echo "usage: $0 [Debug|Release] [avx2|sse4.1]" >&2
    exit 2
fi

if [[ "$simd" == "avx2" ]]; then
    simd_flags="-mavx2 -mbmi -mpopcnt -mlzcnt -mf16c"
    use_avx2=ON
    use_sse41=OFF
else
    simd_flags="-msse4.1"
    use_avx2=OFF
    use_sse41=ON
fi

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
physics_root="$repository_root/.deps/physics"
build_root="$physics_root/build"
install_root="$physics_root/install/$configuration"
mkdir -p "$physics_root" "$build_root" "$install_root"

stamp="$install_root/.crowny-physics-version"
expected_stamp=$'box3d=8441b4a06d6d09dcfb0b0f704df4d847d1437b92\njolt=e77f175595e64cb44218cc9d9d56fc365ad0e36a\nbullet3=2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5\nsimd='"$simd"$'-v1'
required_libraries=(
    "$install_root/lib/libbox3d.a"
    "$install_root/lib/libJolt.a"
    "$install_root/lib/libBulletDynamics.a"
    "$install_root/lib/libBulletCollision.a"
    "$install_root/lib/libLinearMath.a"
)

if [[ -f "$stamp" ]] && [[ "$(<"$stamp")" == "$expected_stamp" ]]; then
    missing_library=false
    for library in "${required_libraries[@]}"; do
        if [[ ! -f "$library" ]]; then
            missing_library=true
            break
        fi
    done
    if [[ "$missing_library" == false ]]; then
        echo "Physics dependencies are already built for $configuration."
        exit 0
    fi
fi

safe_remove_tree() {
    local target="$1"
    case "$target" in
        "$physics_root"/*) rm -rf -- "$target" ;;
        *) echo "refusing to remove path outside $physics_root: $target" >&2; exit 1 ;;
    esac
}

ensure_dependency() {
    local name="$1" repository="$2" commit="$3" required="$4"
    local target="$physics_root/$name"
    if [[ -f "$target/$required" ]] && [[ "$(git -C "$target" rev-parse HEAD 2>/dev/null || true)" == "$commit" ]]; then
        return
    fi

    local staging="$physics_root/staging-$name"
    safe_remove_tree "$staging"
    git clone --filter=blob:none --no-checkout "$repository" "$staging"
    git -C "$staging" fetch --depth 1 origin "$commit"
    git -C "$staging" checkout --detach "$commit"
    [[ "$(git -C "$staging" rev-parse HEAD)" == "$commit" && -f "$staging/$required" ]]
    safe_remove_tree "$target"
    mv -- "$staging" "$target"
}

ensure_dependency box3d https://github.com/erincatto/box3d.git 8441b4a06d6d09dcfb0b0f704df4d847d1437b92 include/box3d/box3d.h
ensure_dependency jolt https://github.com/jrouwe/JoltPhysics.git e77f175595e64cb44218cc9d9d56fc365ad0e36a Jolt/Jolt.h
ensure_dependency bullet3 https://github.com/bulletphysics/bullet3.git 2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5 src/btBulletDynamicsCommon.h

cmake -S "$physics_root/box3d" -B "$build_root/box3d-$configuration" \
    -DCMAKE_BUILD_TYPE="$configuration" -DCMAKE_C_FLAGS="$simd_flags" -DCMAKE_CXX_FLAGS="$simd_flags" \
    -DBOX3D_SAMPLES=OFF -DBOX3D_UNIT_TESTS=OFF \
    -DBOX3D_BENCHMARKS=OFF -DBOX3D_DOCS=OFF -DBOX3D_VALIDATE=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build "$build_root/box3d-$configuration" --parallel
mkdir -p "$install_root/include/box3d" "$install_root/lib"
cp -R "$physics_root/box3d/include/box3d/." "$install_root/include/box3d/"
cp "$(find "$build_root/box3d-$configuration" -name 'libbox3d.a' -print -quit)" "$install_root/lib/libbox3d.a"

cmake -S "$physics_root/jolt/Build" -B "$build_root/jolt-$configuration" \
    -DCMAKE_BUILD_TYPE="$configuration" -DCMAKE_C_FLAGS="$simd_flags" -DCMAKE_CXX_FLAGS="$simd_flags" \
    -DCMAKE_INSTALL_PREFIX="$install_root" \
    -DINTERPROCEDURAL_OPTIMIZATION=OFF -DENABLE_ALL_WARNINGS=OFF -DENABLE_OBJECT_STREAM=OFF \
    -DDEBUG_RENDERER_IN_DEBUG_AND_RELEASE=OFF -DPROFILER_IN_DEBUG_AND_RELEASE=OFF \
    -DFLOATING_POINT_EXCEPTIONS_ENABLED=OFF \
    -DUSE_SSE4_1="$use_sse41" -DUSE_SSE4_2=OFF -DUSE_AVX=OFF -DUSE_AVX2="$use_avx2" -DUSE_AVX512=OFF \
    -DUSE_LZCNT=OFF -DUSE_TZCNT=OFF -DUSE_F16C=OFF -DUSE_FMADD=OFF \
    -DJPH_USE_DX12=OFF -DJPH_USE_VK=OFF -DJPH_USE_MTL=OFF -DJPH_USE_CPU_COMPUTE=OFF \
    -DTARGET_UNIT_TESTS=OFF -DTARGET_HELLO_WORLD=OFF -DTARGET_PERFORMANCE_TEST=OFF \
    -DTARGET_SAMPLES=OFF -DTARGET_VIEWER=OFF
cmake --build "$build_root/jolt-$configuration" --parallel
cmake --install "$build_root/jolt-$configuration" --prefix "$install_root"

cmake -S "$physics_root/bullet3" -B "$build_root/bullet3-$configuration" \
    -DCMAKE_BUILD_TYPE="$configuration" -DCMAKE_C_FLAGS="$simd_flags" -DCMAKE_CXX_FLAGS="$simd_flags" \
    -DCMAKE_INSTALL_PREFIX="$install_root" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_BULLET2_DEMOS=OFF -DBUILD_CPU_DEMOS=OFF \
    -DBUILD_OPENGL3_DEMOS=OFF -DBUILD_EXTRAS=OFF -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_PYBULLET=OFF -DINSTALL_LIBS=ON
cmake --build "$build_root/bullet3-$configuration" --parallel
cmake --install "$build_root/bullet3-$configuration" --prefix "$install_root"

for library in "${required_libraries[@]}"; do
    [[ -f "$library" ]] || { echo "Missing physics library after build: $library" >&2; exit 1; }
done
printf '%s\n' "$expected_stamp" > "$stamp"
echo "Physics dependencies are ready in $install_root."
