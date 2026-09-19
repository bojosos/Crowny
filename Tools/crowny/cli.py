import argparse
import sys

from . import __version__, bootstrap, cmd, env, log, premake
from .measure import SCENARIOS as _MEASURE_SCENARIOS


def _configuration(value):
    return env.validate_configuration(value)


def _simd(value):
    value = value.lower()
    if value not in env.SIMD_LEVELS:
        raise argparse.ArgumentTypeError(f"choose from {', '.join(env.SIMD_LEVELS)}")
    return value


def _sanitizer(value):
    if value not in env.SANITIZERS:
        raise argparse.ArgumentTypeError(f"choose from {', '.join(env.SANITIZERS)}")
    return value


def build_parser():
    parser = argparse.ArgumentParser(
        prog="crowny", description="Cross-platform Crowny build orchestration."
    )
    parser.add_argument("--version", action="version", version=f"crowny {__version__}")
    parser.add_argument(
        "--root", default="", help="Repository root (default: auto-detect)."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    setup_parser = subparsers.add_parser("setup", help="Bootstrap dependencies and generate projects.")
    setup_parser.add_argument("--build", action="store_true", help="Also build (delegates until phase 2).")
    setup_parser.add_argument("--test", action="store_true", help="Also run tests (requires --build).")
    setup_parser.add_argument("--coreclr", action="store_true", help="Bootstrap the repository-local .NET SDK.")
    setup_parser.add_argument("--configuration", default="Release", type=_configuration)
    setup_parser.add_argument("--sanitizer", default="None", type=_sanitizer)
    setup_parser.add_argument("--simd", default="avx2", type=_simd)
    setup_parser.add_argument("--vulkan-version", default=None)

    gen_parser = subparsers.add_parser("gen", help="Regenerate IDE projects with premake.")
    gen_parser.add_argument("--simd", default="avx2", type=_simd)
    gen_parser.add_argument("--force", action="store_true")

    subparsers.add_parser("doctor", help="Report discovered tools and dependency roots.")

    osl_parser = subparsers.add_parser("osl", help="Compile OSL assets or run GPU validation.")
    osl_commands = osl_parser.add_subparsers(dest="osl_command", required=True)
    osl_compile = osl_commands.add_parser("compile")
    osl_compile.add_argument("source")
    osl_compile.add_argument("destination")
    osl_compile.add_argument("--output", default="Cout")
    osl_compile.add_argument("--spirv-bin")
    osl_tests = osl_commands.add_parser("test")
    osl_tests.add_argument("--spirv-bin")
    osl_tests.add_argument("--no-build", action="store_true")
    osl_tests.add_argument("--runner")
    osl_tests.add_argument("--procedural-only", action="store_true", help="Test GLSL without an OSL installation.")

    build_parser = subparsers.add_parser("build", help="Build engine targets.")
    build_parser.add_argument(
        "target_pos", nargs="?", default=None, choices=[None, "Engine", "Editor", "Player", "Tests", "RenderTests", "All"]
    )
    build_parser.add_argument("--target", dest="target_flag", default=None)
    build_parser.add_argument("--configuration", default="Release", type=_configuration)
    build_parser.add_argument("--sanitizer", default="None", type=_sanitizer)
    build_parser.add_argument("--jobs", default=0, type=int)
    build_parser.add_argument("--simd", default="avx2", type=_simd)
    build_parser.add_argument("--clean", action="store_true")
    build_parser.add_argument("--profile", action="store_true")
    build_parser.add_argument(
        "--skip-editor-resources", action="store_true",
        help="Skip editor resource cooking and player template staging for headless CI; still build native and managed targets.",
    )
    build_parser.add_argument("--compiler-cache", default="None", choices=["None", "Sccache"])
    build_parser.add_argument(
        "--inner-loop",
        action="store_true",
        help="Skip building project references; requires a prior full build.",
    )

    test_parser = subparsers.add_parser("test", help="Build and run Catch2 tests.")
    test_parser.add_argument("--configuration", default="Release", type=_configuration)
    test_parser.add_argument("--sanitizer", default="None", type=_sanitizer)
    test_parser.add_argument("--jobs", default=0, type=int)
    test_parser.add_argument("--simd", default="avx2", type=_simd)
    test_parser.add_argument("--clean", action="store_true")
    test_parser.add_argument("--profile", action="store_true")
    test_parser.add_argument("--compiler-cache", default="None")
    test_parser.add_argument("--filter", default="")
    test_parser.add_argument("--process-isolated", action="store_true")

    managed_parser = subparsers.add_parser("managed", help="Build the managed C# assemblies.")
    managed_parser.add_argument("--configuration", default="Release", type=_configuration)

    render_parser = subparsers.add_parser("render-tests", help="Build and run render tests.")
    render_parser.add_argument("--configuration", default="Release", type=_configuration)
    render_parser.add_argument("--backend", default="All", choices=["All", "Vulkan", "OpenGL"])
    render_parser.add_argument("--sanitizer", default="None", type=_sanitizer)
    render_parser.add_argument("--jobs", default=0, type=int)
    render_parser.add_argument("--filter", default="")
    render_parser.add_argument("--reference-root", default="Crowny-RenderTests/References")
    render_parser.add_argument("--artifact-root", default="artifacts/render-tests")
    render_parser.add_argument("--update-references", action="store_true")
    render_parser.add_argument("--no-build", action="store_true")
    render_parser.add_argument("--benchmark-sprites", choices=["full", "smoke"])

    subparsers.add_parser(
        "sccache-probe", help="Validate sccache through MSBuild and record the feasibility stamp."
    )

    measure_parser = subparsers.add_parser("measure", help="Run build measurement scenarios.")
    measure_parser.add_argument(
        "--scenario", action="append", default=None, choices=list(_MEASURE_SCENARIOS)
    )
    measure_parser.add_argument("--target", default="Tests", choices=["Engine", "Editor", "Player", "Tests", "RenderTests", "All"])
    measure_parser.add_argument("--configuration", default="Release", type=_configuration)
    measure_parser.add_argument("--sanitizer", default="None", type=_sanitizer)
    measure_parser.add_argument("--jobs", default=0, type=int)
    measure_parser.add_argument("--simd", default="avx2", type=_simd)
    measure_parser.add_argument("--compiler-cache", default="None", choices=["None", "Sccache"])

    deps_parser = subparsers.add_parser("deps", help="Bootstrap a single dependency.")
    deps_sub = deps_parser.add_subparsers(dest="dependency", required=True)

    osl_deps = deps_sub.add_parser("osl")
    osl_deps.add_argument("--deps-prefix", help="Use an existing LLVM/Clang/OIIO dependency prefix instead of micromamba.")
    osl_deps.add_argument("--micromamba", help="Path to micromamba if it is not on PATH.")
    osl_deps.add_argument("--jobs", type=int, default=2)

    vulkan_parser = deps_sub.add_parser("vulkan")
    vulkan_parser.add_argument("--version", default=None)
    vulkan_parser.add_argument("--force", action="store_true")

    openal_parser = deps_sub.add_parser("openal")
    openal_parser.add_argument("--configuration", default="Release", type=_configuration)
    openal_parser.add_argument("--simd", default="avx2", type=_simd)
    openal_parser.add_argument("--force", action="store_true")

    physics_parser = deps_sub.add_parser("physics")
    physics_parser.add_argument("--configuration", default="Release", type=_configuration)
    physics_parser.add_argument("--simd", default="avx2", type=_simd)
    physics_parser.add_argument("--force", action="store_true")

    spirv_parser = deps_sub.add_parser("spirv-cross")
    spirv_parser.add_argument("--configuration", default="Release", type=_configuration)
    spirv_parser.add_argument("--simd", default="avx2", type=_simd)
    spirv_parser.add_argument("--vulkan-version", default=None)
    spirv_parser.add_argument("--force", action="store_true")

    dotnet_parser = deps_sub.add_parser("dotnet")
    dotnet_parser.add_argument("--architecture", default="x64", choices=["x64", "arm64"])
    dotnet_parser.add_argument("--version", default="")
    dotnet_parser.add_argument("--install-dir", default="")

    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    root = args.root or None

    try:
        if args.command == "osl":
            from . import osl

            if args.osl_command == "compile":
                osl.cook(root, args.source, args.destination, args.output, args.spirv_bin)
            else:
                osl.run_tests(root, args.spirv_bin, args.no_build, args.runner, args.procedural_only)
            return 0

        if args.command == "doctor":
            from . import doctor

            return doctor.doctor()

        if args.command == "setup":
            bootstrap.setup(
                root=root,
                build=args.build,
                test=args.test,
                coreclr=args.coreclr,
                configuration=args.configuration,
                sanitizer=args.sanitizer,
                simd=args.simd,
                vulkan_version=args.vulkan_version,
            )
            return 0

        if args.command == "gen":
            premake.ensure_projects(root, simd=args.simd, force=args.force)
            return 0

        if args.command == "build":
            from . import build

            build.build(
                root=root,
                target=args.target_flag or args.target_pos or "Engine",
                configuration=args.configuration,
                sanitizer=args.sanitizer,
                jobs=args.jobs,
                clean=args.clean,
                profile=args.profile,
                compiler_cache=args.compiler_cache,
                simd=args.simd,
                inner_loop=args.inner_loop,
                skip_editor_resources=args.skip_editor_resources,
            )
            return 0

        if args.command == "test":
            from . import catch2

            catch2.run(
                root=root,
                configuration=args.configuration,
                sanitizer=args.sanitizer,
                jobs=args.jobs,
                clean=args.clean,
                profile=args.profile,
                compiler_cache=args.compiler_cache,
                simd=args.simd,
                process_isolated=args.process_isolated,
                filter=args.filter,
            )
            return 0

        if args.command == "managed":
            from . import managed

            managed.ensure(root, args.configuration)
            return 0

        if args.command == "render-tests":
            from . import render

            render.run(
                root=root,
                configuration=args.configuration,
                backend=args.backend,
                sanitizer=args.sanitizer,
                jobs=args.jobs,
                filter=args.filter,
                reference_root=args.reference_root,
                artifact_root=args.artifact_root,
                update_references=args.update_references,
                no_build=args.no_build,
                benchmark_sprites=args.benchmark_sprites,
            )
            return 0

        if args.command == "sccache-probe":
            from . import sccache

            sccache.probe(root)
            return 0

        if args.command == "measure":
            from . import measure

            measure.measure(
                root=root,
                scenarios=args.scenario or list(measure.SCENARIOS),
                target=args.target,
                configuration=args.configuration,
                sanitizer=args.sanitizer,
                jobs=args.jobs,
                simd=args.simd,
                compiler_cache=args.compiler_cache,
            )
            return 0

        if args.command == "deps":
            from .deps import dotnet, openal, physics, spirvcross, vulkan

            if args.dependency == "vulkan":
                vulkan.ensure(root, version=args.version)
            elif args.dependency == "openal":
                openal.ensure(root, configuration=args.configuration, simd=args.simd, force=args.force)
            elif args.dependency == "physics":
                physics.ensure(root, configuration=args.configuration, simd=args.simd, force=args.force)
            elif args.dependency == "spirv-cross":
                spirvcross.ensure(
                    root,
                    configuration=args.configuration,
                    version=args.vulkan_version,
                    simd=args.simd,
                    force=args.force,
                )
            elif args.dependency == "dotnet":
                dotnet.ensure(root, version=args.version, architecture=args.architecture, install_directory=args.install_dir)
            elif args.dependency == "osl":
                from . import osl

                osl.ensure(root, args.deps_prefix, args.micromamba, args.jobs)
            return 0

        parser.error(f"Unknown command: {args.command}")
    except (RuntimeError, cmd.CommandError) as error:
        log.error(str(error))
        return 1
    except KeyboardInterrupt:
        log.error("Interrupted.")
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
