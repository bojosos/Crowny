#!/usr/bin/env python3
"""Build a sample, relocate its game folder, and exercise both desktop renderers."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--no-build", action="store_true", help="Use the existing Release player template and test executable")
    parser.add_argument("--game-directory", type=Path, help="Validate an existing export of the smoke-test sample")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / "Tools"))
    from crowny import locks

    if arguments.game_directory:
        sample = root / "artifacts/standalone-validation"
        sample.mkdir(parents=True, exist_ok=True)
        with locks.exclusive_lock(root, "render-test-runtime"):
            verify_game(sample, os.environ, arguments.game_directory.resolve())
        return

    tool = [sys.executable, "-u", str(root / "Tools/crowny")]
    if not arguments.no_build:
        subprocess.run(tool + ["build", "Player", "--configuration", "Release"], cwd=root, check=True)
    sample = root / "artifacts/standalone-sample"
    environment = os.environ.copy()
    environment["CROWNY_PLAYER_TEMPLATE"] = str(root / "bin/Release-windows-x86_64/PlayerTemplate")
    environment["CROWNY_PLAYER_SMOKE_ROOT"] = str(sample)
    if arguments.no_build:
        sys.path.insert(0, str(root / "Tools"))
        from crowny import catch2, env, locks

        env.configure_default_environment(root)
        with locks.output_read_lock(root, "Release"):
            executable, test_output = catch2.test_executable(root, "Release")
            runtime = catch2._runtime_environment(root, test_output)
            runtime.update({key: value for key, value in environment.items() if key.startswith("CROWNY_PLAYER_")})
            subprocess.run([str(executable), "[PlayerSmoke]"], cwd=root, env=runtime, check=True)
    else:
        subprocess.run(tool + ["test", "--filter", "[PlayerSmoke]"], cwd=root, env=environment, check=True)
    with locks.exclusive_lock(root, "render-test-runtime"):
        verify_game(sample, environment)


def verify_game(sample, environment, game_source=None):
    environment = environment.copy()
    game_source = game_source or sample / "Game"
    with tempfile.TemporaryDirectory(prefix="crowny-standalone-launch-") as directory:
        isolated = Path(directory)
        game = isolated / "Game with spaces"
        shutil.copytree(game_source, game)
        for name in list(environment):
            if name.startswith(("CROWNY_", "MONO_", "DOTNET_")):
                environment.pop(name)
        environment["PATH"] = str(Path(environment.get("SystemRoot", "C:/Windows")) / "System32")
        for renderer in ("vulkan", "opengl"):
            for marker in ("script-started.txt", "script-updated.txt"):
                (game / marker).unlink(missing_ok=True)
            report = sample / f"{renderer}.txt"
            report.unlink(missing_ok=True)
            command = [str(game / "Game.exe"), "--frames", "30", "--hidden", "--report", str(report)]
            if renderer == "opengl":
                command.append("--opengl")
            try:
                with (sample / f"{renderer}.log").open("w", encoding="utf-8") as log:
                    subprocess.run(command, cwd=isolated, env=environment, stdout=log,
                                   stderr=subprocess.STDOUT, check=True, timeout=120)
            finally:
                for log_path in game.glob("*.log"):
                    shutil.copy2(log_path, sample / f"{renderer}-{log_path.name}")
            if "Player completed successfully" not in report.read_text():
                raise RuntimeError(f"{renderer}: player did not report a rendered frame")
            for marker in ("script-started.txt", "script-updated.txt"):
                if not (game / marker).is_file():
                    raise RuntimeError(f"{renderer}: missing managed lifecycle marker {marker}")
            print(f"PASS: relocated {renderer} player rendered and ran managed Start/Update")
    print(f"Interactive sample: {game_source / 'Game.exe'}")


if __name__ == "__main__":
    main()
