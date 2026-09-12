import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[3] / "Scripts/countlines.py"
SPEC = importlib.util.spec_from_file_location("countlines", SCRIPT)
countlines = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(countlines)


class CountLinesTests(unittest.TestCase):
    def test_discovery_counts_owned_source_once_and_excludes_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            included = {
                "Crowny-Player/Source/Main.cpp", "Crowny-Tests/Source/Test.cpp",
                "Tools/new.py", "Scripts/crowny", "premake5.lua",
                "Crowny-Editor/Resources/Shaders/Test.glslinc",
                "Crowny/Source/Crowny/Scripting/Backends/Generated/GeneratedMetadataBackend.cpp",
            }
            excluded = {
                "Crowny/Dependencies/glfw/test.cpp", "3rdparty/test.py",
                "Crowny/Source/Vendor/fastlz/fastlz.cpp", "bin/output.cs",
                "Crowny-Managed/obj/output.cs", "README.md", "texture.png",
                "Crowny-Sharp/Source/Runtime/Generated/ManagedHostApi.g.cs",
                "Crowny/Source/Crowny/Common/UnicodeGraphemeData.inl",
            }
            for name in included | excluded:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("test\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True)
            (root / ".gitignore").write_text("ignored.py\n", encoding="utf-8")
            (root / "ignored.py").write_text("ignored\n", encoding="utf-8")
            (root / "new.cs").write_text("new\n", encoding="utf-8")
            # A tracked file deleted from the working tree is not counted.
            (root / "Tools/new.py").unlink()
            actual = list(countlines.source_files(root))
            self.assertEqual(set(actual), (included - {"Tools/new.py"}) | {"new.cs"})
            self.assertEqual(len(actual), len(set(actual)))

    def test_utf8_bom_crlf_blank_lines_and_unterminated_last_line(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.cpp"
            path.write_bytes(b"\xef\xbb\xbf// \xe2\x80\x9d\r\n \t\r\nx")
            self.assertEqual(countlines.count_file(path), (3, 2, 9))
            path.write_bytes(b"")
            self.assertEqual(countlines.count_file(path), (0, 0, 0))


if __name__ == "__main__":
    unittest.main()
