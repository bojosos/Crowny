import sys

from . import build as build_module
from . import cmd, env, locks
from .build import output_dir
from .locks import exclusive_lock


def _executable(root, output_configuration):
    executable = (
        output_dir(root, output_configuration)
        / "Crowny-RenderTests"
        / ("Crowny-RenderTests.exe" if sys.platform == "win32" else "Crowny-RenderTests")
    )
    if not executable.is_file():
        raise RuntimeError(f"Render test executable was not found: {executable}")
    return executable


def run(
    root=None,
    configuration="Release",
    backend="All",
    sanitizer="None",
    jobs=0,
    filter="",
    reference_root="Crowny-RenderTests/References",
    artifact_root="artifacts/render-tests",
    update_references=False,
    no_build=False,
):
    root = root or env.repo_root()
    if backend not in ("All", "Vulkan", "OpenGL"):
        raise ValueError(f"Unsupported backend: {backend}")

    if not no_build:
        build_module.build(
            root=root,
            target="RenderTests",
            configuration=configuration,
            sanitizer=sanitizer,
            jobs=jobs,
        )

    workspace_config = env.workspace_configuration(configuration, sanitizer)
    output_configuration = env.output_configuration(configuration, sanitizer)
    selected = {"Vulkan": ["vulkan"], "OpenGL": ["opengl"]}.get(backend, ["vulkan", "opengl"])

    references = root / reference_root
    artifacts = root / artifact_root

    with locks.output_read_lock(root, workspace_config), exclusive_lock(root, "render-test-runtime"):
        executable = _executable(root, output_configuration)

        if update_references:
            if backend == "OpenGL":
                raise RuntimeError("Reference updates use Vulkan. Select Vulkan or All.")
            arguments = [
                str(executable),
                "--backend",
                "vulkan",
                "--references",
                str(references),
                "--artifacts",
                str(artifacts),
                "--update-references",
            ]
            if filter:
                arguments += ["--filter", filter]
            cmd.run_checked(arguments)

        for renderer in selected:
            arguments = [
                str(executable),
                "--backend",
                renderer,
                "--references",
                str(references),
                "--artifacts",
                str(artifacts),
            ]
            if filter:
                arguments += ["--filter", filter]
            cmd.run_checked(arguments)

        if len(selected) == 2:
            cmd.run_checked(
                [
                    str(executable),
                    "--compare-backends",
                    str(artifacts / "vulkan"),
                    str(artifacts / "opengl"),
                    "--artifacts",
                    str(artifacts / "backend-diff"),
                ]
            )
