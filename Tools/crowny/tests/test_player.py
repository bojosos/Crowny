import _bootstrap  # noqa: F401

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from crowny import player


class PlayerTemplateTests(unittest.TestCase):
    def test_stages_player_content_mono_api_and_crt_without_editor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "bin/Release-windows-x86_64/Crowny-Player"
            mono = root / "MonoSDK"
            compiler = root / "VS/VC/Tools/MSVC/14/bin/Hostx64/x64/cl.exe"
            files = [source / "Crowny-Player.exe", source / "mono-2.0-sgen.dll", source / "shaderc_shared.dll",
                     root / "Crowny-Editor/Resources/Builtin.cwpack",
                     root / ".deps/generated/managed/Release/CrownySharp.dll",
                     mono / "lib/mono/4.5/mscorlib.dll", mono / "etc/mono/config",
                     mono / "lib/mono/gac/System.Core/4.0.0.0__b77a5c561934e089/System.Core.dll",
                     root / "VS/VC/Redist/MSVC/14/x64/Microsoft.VC143.CRT/msvcp140.dll"]
            for file in files:
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b"runtime fixture")
            with mock.patch.object(player.sys, "platform", "win32"), mock.patch.object(
                player.env, "platform_tag", return_value="windows-x86_64"
            ), mock.patch.object(player.msbuild, "find_msvc_compiler", return_value=compiler), mock.patch.dict(
                "os.environ", {"CROWNY_MONO_ROOT": str(mono)}
            ), mock.patch.object(player.resources, "_builtins_need_cooking", return_value=False) as cooking:
                player.stage_template(root, "Release", "Release")
                cooking.return_value = True
                (source / "Crowny-Player.exe").write_bytes(b"new executable")
                with self.assertRaisesRegex(RuntimeError, "rendering resources are incomplete or stale"):
                    player.stage_template(root, "Release", "Release")
            template = root / "bin/Release-windows-x86_64/PlayerTemplate"
            for path in ("Game.exe", "Managed/CrownySharp.dll", "Mono/lib/mono/4.5/mscorlib.dll",
                         "Mono/etc/mono/config", "Resources/Builtin.cwpack", "msvcp140.dll", "shaderc_shared.dll",
                         "Mono/lib/mono/gac/System.Core/4.0.0.0__b77a5c561934e089/System.Core.dll"):
                self.assertEqual((template / path).read_bytes(), b"runtime fixture")
            self.assertFalse((template / "Crowny-Editor.exe").exists())

    def test_missing_inputs_fail_before_publishing_a_template(self):
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(player.sys, "platform", "win32"):
            with self.assertRaisesRegex(RuntimeError, "Player runtime input is missing"):
                player.stage_template(Path(temporary), "Release", "Release")


if __name__ == "__main__":
    unittest.main()
