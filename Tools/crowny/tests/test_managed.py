import _bootstrap  # noqa: F401

import tempfile
import unittest
from pathlib import Path

from crowny import env, managed


class FastNoisePatchTests(unittest.TestCase):
    def test_unsupported_constant_is_patched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_dir = root / "Crowny" / "Dependencies" / "FastNoiseLite" / "CSharp"
            source_dir.mkdir(parents=True)
            source = source_dir / "FastNoiseLite.cs"
            source.write_text("public class F { " + managed._FASTNOISE_UNSUPPORTED + " }", encoding="utf-8")

            generated = managed._fastnoise_source(root)
            self.assertEqual(generated.name, "FastNoiseLite.Mono.cs")
            self.assertIn("private const short OPTIMISE = 0;", generated.read_text(encoding="utf-8"))

    def test_compatible_source_is_returned_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_dir = root / "Crowny" / "Dependencies" / "FastNoiseLite" / "CSharp"
            source_dir.mkdir(parents=True)
            source = source_dir / "FastNoiseLite.cs"
            source.write_text("public class F { }", encoding="utf-8")

            self.assertEqual(managed._fastnoise_source(root), source)


class ManagedDefineTests(unittest.TestCase):
    def test_all_configurations_have_defines(self):
        for configuration in env.CONFIGURATIONS:
            self.assertIn(configuration, managed._MANAGED_DEFINES)


if __name__ == "__main__":
    unittest.main()
