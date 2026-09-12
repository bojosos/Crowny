import importlib.util
import json
import re
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[3] / "Scripts" / "managed" / "generate_managed_interop.py"
SPEC = importlib.util.spec_from_file_location("generate_managed_interop", SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


def target_source(source, mono):
    """Select the generated runtime branch without requiring either C# toolchain."""
    enabled = True
    lines = []
    for line in source.splitlines():
        directive = line.strip()
        if directive == "#if CROWNY_MONO":
            enabled = mono
        elif directive == "#else":
            enabled = not mono
        elif directive == "#endif":
            enabled = True
        elif directive.startswith("#"):
            raise AssertionError(f"Unexpected generated preprocessor directive: {directive}")
        elif enabled:
            lines.append(line)
    return "\n".join(lines)


class ManagedTransportGenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(GENERATOR.MANIFEST.read_text(encoding="utf-8"))
        cls.functions = GENERATOR.expand_host_functions(cls.manifest)
        cls.source = GENERATOR.generate_typed_host_api(cls.manifest)

    def test_coreclr_uses_cdecl_function_pointers_for_every_binding(self):
        source = target_source(self.source, mono=False)
        self.assertFalse("GetDelegateForFunctionPointer" in source, "CoreCLR still marshals delegates")
        self.assertFalse("UnmanagedFunctionPointer" in source, "CoreCLR still declares marshaled delegates")
        declarations = re.findall(r"private static delegate\* unmanaged\[Cdecl\]<[^;]+> (\w+)Callback;", source)
        self.assertEqual(set(declarations), set(self.functions))
        for name in self.functions:
            with self.subTest(function=name):
                self.assertIsNotNone(re.search(rf"{name}Callback = \(delegate\* unmanaged\[Cdecl\]<[^;]+>\)value\.{name};", source))

    def test_function_pointer_signatures_preserve_abi_value_and_pointer_parameters(self):
        source = target_source(self.source, mono=False)
        expected = {
            "TimeGetDeltaTime": "void*, float*, int",
            "TransformGetPosition": "void*, ManagedNativeUuid, ManagedNativeVec3*, int",
            "TransformSetPosition": "void*, ManagedNativeUuid, ManagedNativeVec3*, int",
            "SetEntityName": "void*, ManagedNativeUuid, ManagedNativeStringView, int",
        }
        for name, signature in expected.items():
            with self.subTest(function=name):
                self.assertTrue(f"delegate* unmanaged[Cdecl]<{signature}> {name}Callback;" in source, name)

    def test_mono_keeps_cdecl_delegates_and_has_no_function_pointer_syntax(self):
        source = target_source(self.source, mono=True)
        self.assertFalse("delegate*" in source, "Mono cannot parse C# function pointers")
        self.assertTrue("[UnmanagedFunctionPointer(CallingConvention.Cdecl)]" in source)
        bindings = re.findall(r"(\w+)Callback = Marshal.GetDelegateForFunctionPointer<\w+>\(value\.\w+\);", source)
        self.assertEqual(set(bindings), set(self.functions))

    def test_both_targets_clear_every_callback_and_preserve_table_layout_and_calls(self):
        mono = target_source(self.source, mono=True)
        coreclr = target_source(self.source, mono=False)
        for source in (mono, coreclr):
            clear = source.split("if (value.Context == IntPtr.Zero)", 1)[1].split("return;", 1)[0]
            self.assertEqual(set(re.findall(r"(\w+)Callback = null;", clear)), set(self.functions))
            self.assertIn("api = value;", clear)
        start = "internal unsafe struct ManagedNativeHostApi"
        end = "internal static unsafe class ManagedHostTransport"
        self.assertEqual(mono.split(start, 1)[1].split(end, 1)[0], coreclr.split(start, 1)[1].split(end, 1)[0])
        self.assertEqual(
            re.findall(r"internal static int .*Callback\(.*", mono),
            re.findall(r"internal static int .*Callback\(.*", coreclr),
        )


if __name__ == "__main__":
    unittest.main()
