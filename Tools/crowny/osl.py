"""Offline OSL toolchain acquisition, compilation and GPU validation."""
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

from . import cmd, env, locks, log


def executable(name):
    return name + (".exe" if sys.platform == "win32" else "")


def manifest(root):
    return json.loads((root / "Tools/osl/toolchain.json").read_text(encoding="utf-8"))


def toolchain_root(root):
    return Path(os.environ.get("CROWNY_OSL_ROOT") or env.deps_root(root) / "osl").resolve()


def host_tag():
    machine = platform.machine().lower()
    architecture = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    return f"{env.host_platform()}-{architecture}"


def spirv_directory(value=None):
    if value:
        directory = Path(value).resolve()
    elif os.environ.get("VULKAN_SDK"):
        sdk = Path(os.environ["VULKAN_SDK"])
        directory = sdk / ("Bin" if sys.platform == "win32" else "bin")
    else:
        found = shutil.which("spirv-val")
        if not found:
            raise RuntimeError("Set VULKAN_SDK or pass --spirv-bin with the Vulkan SDK tools directory.")
        directory = Path(found).parent
    for name in ("glslangValidator", "spirv-as", "spirv-dis", "spirv-link", "spirv-opt", "spirv-val"):
        if not (directory / executable(name)).is_file():
            raise RuntimeError(f"Missing {name} in {directory}")
    return directory.resolve()


def compiler_environment(prefix, install):
    """Only compiler children get OSL/LLVM libraries; never modify the editor environment."""
    result = os.environ.copy()
    paths = [install / "bin", prefix / "bin"]
    if sys.platform == "win32":
        paths += [prefix.parent, prefix.parent / "Scripts"]
    result["PATH"] = os.pathsep.join(map(str, paths)) + os.pathsep + result.get("PATH", "")
    library_variable = "DYLD_LIBRARY_PATH" if sys.platform == "darwin" else "LD_LIBRARY_PATH"
    if sys.platform != "win32":
        result[library_variable] = os.pathsep.join(map(str, [install / "lib", install / "lib64", prefix / "lib", prefix / "lib64"])) + (
            os.pathsep + result[library_variable] if result.get(library_variable) else "")
    # User OSL_OPTIONS from another renderer/experiment must not change the cooker.
    result.pop("OSL_OPTIONS", None)
    return result


def windows_build_environment(root, result):
    if sys.platform != "win32":
        return result
    from . import msbuild
    compiler = msbuild.find_msvc_compiler(root)
    vc_root = compiler.parents[3]
    vs_root = vc_root.parents[3]
    sdk = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits/10"
    versions = sorted((sdk / "Include").glob("10.*"), key=lambda p: tuple(map(int, p.name.split("."))))
    if not versions:
        raise RuntimeError("Install the Windows 10/11 SDK with the Visual Studio C++ build tools.")
    version = versions[-1].name
    result["INCLUDE"] = os.pathsep.join(map(str, [vc_root / "include"] + [sdk / "Include" / version / p for p in ("ucrt", "shared", "um", "winrt")]))
    result["LIB"] = os.pathsep.join(map(str, [vc_root / "lib/x64", sdk / "Lib" / version / "ucrt/x64", sdk / "Lib" / version / "um/x64"]))
    result["PATH"] += os.pathsep + os.pathsep.join(map(str, [compiler.parent, sdk / "bin" / version / "x64"]))
    result.update(VSINSTALLDIR=str(vs_root), VCToolsInstallDir=str(vc_root) + "/", WindowsSdkDir=str(sdk) + "/", WindowsSDKVersion=version + "/")
    return result


def acquire_source(root, destination, specification):
    """Own one pinned checkout. Never reset user edits or follow a moving branch."""
    if not (destination / ".git").is_dir():
        if destination.exists() and any(destination.iterdir()):
            raise RuntimeError(f"Refusing to initialize nonempty OSL source directory: {destination}")
        cmd.run_checked(["git", "init", destination])
        cmd.run_checked(["git", "-C", destination, "remote", "add", "origin", specification["repository"]])
    revision = specification["revision"]
    current = subprocess.run(["git", "-C", str(destination), "rev-parse", "HEAD"], capture_output=True, text=True)
    if current.returncode != 0:
        cmd.run_checked(["git", "-C", destination, "fetch", "--depth=1", "origin", revision])
        cmd.run_checked(["git", "-C", destination, "checkout", "--detach", revision])
    elif current.stdout.strip() != revision:
        raise RuntimeError(f"OSL checkout at {destination} is not pinned revision {revision}; use a new CROWNY_OSL_ROOT.")
    for name in specification["patches"]:
        patch = root / "Tools/osl" / name
        applied = subprocess.run(["git", "-C", str(destination), "apply", "--reverse", "--check", str(patch)], capture_output=True)
        if applied.returncode != 0:
            cmd.run_checked(["git", "-C", destination, "apply", "--check", patch])
            cmd.run_checked(["git", "-C", destination, "apply", patch])


def acquire_dependencies(root, directory, specification, deps_prefix=None, micromamba=None):
    if deps_prefix:
        return Path(deps_prefix).resolve()
    prefix = directory / "environment"
    marker = prefix / "crowny-dependencies.json"
    lock_file = root / "Tools/osl/locks" / (host_tag() + ".txt")
    requested = {"packages": specification["packages"] + specification["platform_packages"][env.host_platform()],
                 "lock": lock_file.read_text(encoding="utf-8") if lock_file.is_file() else None}
    if not marker.is_file() or json.loads(marker.read_text(encoding="utf-8")) != requested:
        manager = micromamba or shutil.which("micromamba")
        if not manager:
            raise RuntimeError("Install micromamba and put it on PATH, pass --micromamba, or supply --deps-prefix with LLVM/Clang 20.1.8 and OIIO 3 dependencies.")
        if not shutil.which(str(manager)):
            raise RuntimeError(f"Micromamba executable was not found: {manager}")
        command = [manager, "--no-rc", "create", "--yes", "--root-prefix", directory / "mamba", "--prefix", prefix]
        if lock_file.is_file():
            command += ["--file", lock_file]
        else:
            log.info(f"No validated dependency lock for {host_tag()}; resolving versioned development packages.")
            command += ["--override-channels", "--channel", "conda-forge", "--strict-channel-priority", *requested["packages"]]
        cmd.run_checked(command)
        marker.write_text(json.dumps(requested, indent=2), encoding="utf-8")
    return prefix / "Library" if sys.platform == "win32" else prefix


def ensure(root=None, deps_prefix=None, micromamba=None, jobs=2):
    root = Path(root or env.repo_root()).resolve()
    directory = toolchain_root(root)
    specification = manifest(root)
    if jobs < 1:
        raise RuntimeError("--jobs must be positive")
    if sys.platform == "win32" and host_tag() != "windows-x86_64":
        raise RuntimeError("The Windows OSL bootstrap currently requires an x64 host.")
    for tool in ("git", "cmake"):
        if not shutil.which(tool):
            raise RuntimeError(f"Install {tool} and put it on PATH before running `crowny deps osl`.")
    with locks.exclusive_lock(root, "osl-toolchain", scope="Shared"):
        directory.mkdir(parents=True, exist_ok=True)
        prefix = acquire_dependencies(root, directory, specification, deps_prefix, micromamba)
        for name in ("llc", "clang++", "clang-cl" if sys.platform == "win32" else "clang"):
            if not (prefix / "bin" / executable(name)).is_file():
                raise RuntimeError(f"Missing {name} in dependency prefix {prefix}. Supply the LLVM/Clang development installation.")
        install = directory / "install"
        build_env = windows_build_environment(root, compiler_environment(prefix, install))
        version = cmd.run_checked([prefix / "bin" / executable("llc"), "--version"], env=build_env, capture=True).stdout
        if specification["llvm"] not in version or "spirv" not in version.lower():
            raise RuntimeError(f"LLVM {specification['llvm']} with the SPIR-V target is required.")
        clang = prefix / "bin" / executable("clang++")
        clang_version = cmd.run_checked([clang, "--version"], env=build_env, capture=True).stdout
        if specification["llvm"] not in clang_version:
            raise RuntimeError("Clang and LLVM must match the pinned version.")
        source = directory / "source"
        acquire_source(root, source, specification)
        if sys.platform == "win32":
            # LLVM's conda import metadata uses this name; keep the alias local.
            compat = directory / "compat/lib"
            compat.mkdir(parents=True, exist_ok=True)
            if (prefix / "lib/zstd.lib").is_file():
                shutil.copy2(prefix / "lib/zstd.lib", compat / "zstd.dll.lib")
            build_env["LIB"] = str(compat) + os.pathsep + str(prefix / "lib") + os.pathsep + build_env["LIB"]
        compiler = prefix / "bin" / executable("clang-cl" if sys.platform == "win32" else "clang")
        cxx = compiler if sys.platform == "win32" else clang
        common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_C_COMPILER={compiler}", f"-DCMAKE_CXX_COMPILER={cxx}"]
        with locks.compiler_lease(root, jobs) as lease:
            # Clear find_package caches so switching dependency prefixes cannot
            # silently keep linking the previous installation's libraries.
            cmd.run_checked(["cmake", "--fresh", "-S", source, "-B", directory / "build", *common,
                f"-DCMAKE_INSTALL_PREFIX={install}", f"-DCMAKE_PREFIX_PATH={prefix}", f"-DLLVM_ROOT={prefix}",
                "-DUSE_PYTHON=OFF", "-DUSE_QT=OFF", "-DUSE_PARTIO=OFF", "-DOSL_USE_OPTIX=OFF",
                "-DOSL_BUILD_TESTS=OFF", "-DOSL_BUILD_DOCS=OFF", "-DSTOP_ON_WARNING=OFF"], env=build_env)
            cmd.run_checked(["cmake", "--build", directory / "build", "--target", "install", "--parallel", lease["jobs"]], env=build_env)
            cmd.run_checked(["cmake", "--fresh", "-S", root / "Tools/osl", "-B", directory / "adapter", *common,
                f"-DCMAKE_PREFIX_PATH={install};{prefix}"], env=build_env)
            cmd.run_checked(["cmake", "--build", directory / "adapter", "--parallel", lease["jobs"]], env=build_env)
            cmd.run_checked([clang, "-std=c++17", "-O2", "-emit-llvm", "-c", "-fno-math-errno", "-ffp-contract=off",
                "-fno-vectorize", "-fno-slp-vectorize", "-DNDEBUG", f"-I{install / 'include'}", f"-I{prefix / 'include'}",
                f"-I{prefix / 'include/Imath'}", root / "Tools/osl/Shadeops.cpp", "-o", directory / "shadeops.bc"], env=build_env)
        # A machine-local description, published only after the complete build.
        configuration = {"schema": 1, "host": host_tag(), "revision": specification["revision"], "deps_prefix": str(prefix)}
        (directory / "toolchain.json").write_text(json.dumps(configuration, indent=2), encoding="utf-8")
        log.info(f"OSL compiler ready in {directory}")
    return directory


def load_toolchain(root):
    directory = toolchain_root(root)
    path = directory / "toolchain.json"
    if not path.is_file():
        raise RuntimeError("OSL compiler is not installed. Run `crowny deps osl` first, or set CROWNY_OSL_ROOT.")
    configuration = json.loads(path.read_text(encoding="utf-8"))
    if configuration.get("schema") != 1 or configuration.get("host") != host_tag() or configuration.get("revision") != manifest(root)["revision"]:
        raise RuntimeError("OSL toolchain does not match this checkout/host. Run `crowny deps osl`.")
    prefix = Path(configuration["deps_prefix"])
    required = [directory / "adapter" / executable("crowny-osl"), directory / "install/bin" / executable("oslc"),
                directory / "shadeops.bc", prefix / "bin" / executable("llc")]
    for path in required:
        if not path.is_file():
            raise RuntimeError(f"OSL compiler installation is incomplete: {path}. Run `crowny deps osl`.")
    return directory, compiler_environment(prefix, directory / "install")


def compile_command(root, source, destination, output="Cout", spirv_bin=None, test_reference=False):
    directory, process_env = load_toolchain(root)
    command = [sys.executable, root / "Tools/osl/compile.py", Path(source).resolve(), Path(destination).resolve(),
        "--output", output, "--compiler", directory / "adapter" / executable("crowny-osl"),
        "--shadeops", directory / "shadeops.bc", "--oslc", directory / "install/bin" / executable("oslc"),
        "--llc", Path(json.loads((directory / "toolchain.json").read_text())["deps_prefix"]) / "bin" / executable("llc"),
        "--spirv-bin", spirv_directory(spirv_bin)]
    if test_reference:
        command += ["--test-reference-generator", directory / "adapter" / executable("crowny-osl-reference")]
    return command, process_env


def cook(root=None, source=None, destination=None, output="Cout", spirv_bin=None):
    root = Path(root or env.repo_root()).resolve()
    command, process_env = compile_command(root, source, destination, output, spirv_bin)
    cmd.run_checked(command, env=process_env)


def run_tests(root=None, spirv_bin=None, no_build=False, runner=None, procedural_only=False):
    root = Path(root or env.repo_root()).resolve()
    spirv_bin = spirv_directory(spirv_bin)
    if not no_build:
        from . import build
        build.build(root=root, target="RenderTests")
    runner = Path(runner).resolve() if runner else root / "bin" / f"Release-{host_tag()}" / "Crowny-RenderTests" / executable("Crowny-RenderTests")
    if not runner.is_file():
        raise RuntimeError(f"Render test executable is missing: {runner}. Build it first or pass --runner.")
    artifacts = root / "artifacts" / ("procedural" if procedural_only else "osl")
    for name, source in (("surface-reference", "reference"), ("checker", "checker"), ("image-reference", "image_reference")):
        cmd.run_checked([sys.executable, root / "Tools/procedural/compile.py", root / f"Tools/procedural/fixtures/{source}.glslinc",
            artifacts / name, "--spirv-bin", spirv_bin], cwd=root)
    if not procedural_only:
        for name, source, output in (("procedural", "procedural", "Cout"), ("derivatives", "procedural", "derivatives"),
                ("noise-derivatives", "noise_derivatives", "Cout"), ("editable", "editable", "Cout"),
                ("mandelbrot", "mandelbrot", "result"), ("image-texture", "image_texture", "Cout")):
            package = artifacts / name
            command, process_env = compile_command(root, root / f"Tools/osl/fixtures/{source}.osl", package, output, spirv_bin, True)
            cmd.run_checked(command, env=process_env)
            if name != "image-texture":
                cmd.run_checked([runner, "--backend", "vulkan", "--osl-package", package, "--artifacts", package / "gpu"], cwd=root)
            reference = "image-reference" if name == "image-texture" else "surface-reference"
            cmd.run_checked([runner, "--backend", "vulkan", "--procedural-package", package,
                "--procedural-reference", artifacts / reference, "--artifacts", package / "surface"], cwd=root)
        negatives = {
            "global": "Only u, v and time globals", "shadeop": "Unsupported device shadeop: osl_exp_ff",
            "parameter": "Unsupported editable OSL input: name", "texture_option": "Unsupported OSL texture option:",
            "texture_derivative": "texture() result derivatives and errormessage are not supported",
            "texture_filename": "texture() filename must be a declared string input",
            "texture_default": "OSL texture inputs require an empty default",
            "texture_string": "may only be used as the filename argument of texture()"}
        for name, diagnostic in negatives.items():
            case = "unsupported_" + name
            command, process_env = compile_command(root, root / f"Tools/osl/fixtures/{case}.osl", artifacts / case, spirv_bin=spirv_bin)
            result = subprocess.run(list(map(str, command)), env=process_env, capture_output=True, text=True)
            report = result.stdout + result.stderr
            (artifacts / f"{case}.log").write_text(report, encoding="utf-8")
            if result.returncode == 0 or diagnostic not in report:
                raise RuntimeError(f"Unsupported shader check failed: {case}\n{report}")
    cmd.run_checked([runner, "--backend", "vulkan", "--procedural-package", artifacts / "checker",
        "--procedural-reference", artifacts / "surface-reference", "--artifacts", artifacts / "checker/surface"], cwd=root)
    log.info("All procedural GPU comparisons and unsupported-feature checks passed.")
