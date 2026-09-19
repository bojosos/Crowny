"""Cook a GLSL procedural texture into Crowny's opaque PBR material.

OSL optionally supplies the implementation through a linked LLVM SPIR-V library.
The engine only loads the resulting validated vertex and fragment modules.
"""
import argparse
import os
import pathlib
import re
import subprocess


def import_osl(assembly):
    """Adapt the known GLSL stub to the value-only OSL ABI, failing on mismatch."""
    names = re.findall(r'OpName (%\d+) "crownyOslEvaluate\((?:vf4;){2,34}"', assembly)
    if len(names) != 1:
        raise ValueError("Expected exactly one unoptimized OSL adapter stub")
    function = names[0]
    pattern = rf"^\s*{function} = OpFunction (%\d+) None (%\d+)\n(.*?)^\s*OpFunctionEnd"
    match = re.search(pattern, assembly, re.MULTILINE | re.DOTALL)
    if not match:
        raise ValueError("OSL adapter function body is missing")
    vector = match[1]
    arguments = re.findall(r"(%\d+) = OpFunctionParameter (%\d+)", match[3])
    if not 2 <= len(arguments) <= 34 or any(kind != arguments[0][1] for _, kind in arguments):
        raise ValueError("OSL adapter requires float4 pointer parameters")
    pointer = arguments[0][1]
    if not re.search(rf"{pointer} = OpTypePointer Function {vector}\s*$", assembly, re.MULTILINE):
        raise ValueError("OSL adapter parameter type mismatch")
    scalar = re.search(rf"{vector} = OpTypeVector (%\d+) 4\s*$", assembly, re.MULTILINE)
    if not scalar or not re.search(rf"{scalar[1]} = OpTypeFloat 32\s*$", assembly, re.MULTILINE):
        raise ValueError("OSL adapter must return float4")

    # Export texture services using value arguments. GLSL's own functions take
    # Function pointers, whereas the LLVM evaluator is deliberately pointer-free.
    exports, decorations = "", ""
    for texture_function, index in re.findall(r'OpName (%\d+) "crownyOslTexture(\d+)\(vf4;vf4;"', assembly):
        texture_match = re.search(rf"^\s*{texture_function} = OpFunction {vector} None (%\d+)\n(.*?)^\s*OpFunctionEnd",
                                  assembly, re.MULTILINE | re.DOTALL)
        if not texture_match or len(re.findall(rf"OpFunctionParameter {pointer}\b", texture_match[2])) != 2:
            raise ValueError("OSL texture service signature mismatch")
        prefix = f"%oslTexture{index}"
        decorations += f'OpDecorate {prefix} LinkageAttributes "crowny_osl_texture_{index}" Export\n'
        exports += (f"{prefix} = OpFunction {vector} None %oslTextureType\n"
                    f"{prefix}Uv = OpFunctionParameter {vector}\n{prefix}Grad = OpFunctionParameter {vector}\n"
                    f"{prefix}Label = OpLabel\n"
                    f"{prefix}UvPtr = OpVariable {pointer} Function\n{prefix}GradPtr = OpVariable {pointer} Function\n"
                    f"OpStore {prefix}UvPtr {prefix}Uv\nOpStore {prefix}GradPtr {prefix}Grad\n"
                    f"{prefix}Result = OpFunctionCall {vector} {texture_function} {prefix}UvPtr {prefix}GradPtr\n"
                    f"OpReturnValue {prefix}Result\nOpFunctionEnd\n")
    # Symbolic identifiers below are new; spirv-as assigns collision-free IDs.
    body = (f"{function} = OpFunction {vector} None {match[2]}\n"
            + "".join(f"{arg} = OpFunctionParameter {kind}\n" for arg, kind in arguments)
            + f"%oslAdapterLabel = OpLabel\n"
            + "".join(f"%oslValue{i} = OpLoad {vector} {arg}\n" for i, (arg, _) in enumerate(arguments))
            + f"%oslResult = OpFunctionCall {vector} %oslImport " + " ".join(f"%oslValue{i}" for i in range(len(arguments))) + "\n"
            + "OpReturnValue %oslResult\nOpFunctionEnd")
    assembly = assembly[:match.start()] + "\n" + body + assembly[match.end():]
    removed_ids = set(re.findall(r"(%\d+) =", match[3])) - {arg for arg, _ in arguments}
    assembly = re.sub(r"^\s*Op(?:Name|Decorate) (%\d+) .*\n",
                      lambda item: "" if item[1] in removed_ids else item[0], assembly, flags=re.MULTILINE)
    assembly = assembly.replace("OpCapability Shader", "OpCapability Shader\nOpCapability Linkage", 1)
    # Decorations precede types; types and imported declarations precede functions.
    first_type = re.search(r"^\s*%\d+ = OpType", assembly, re.MULTILINE).start()
    assembly = (assembly[:first_type] + '\nOpDecorate %oslImport LinkageAttributes "crowny_osl_evaluate_material" Import\n' + decorations
                + assembly[first_type:])
    first_function = re.search(r"^\s*%\d+ = OpFunction ", assembly, re.MULTILINE).start()
    declaration = (f"\n%oslImportType = OpTypeFunction {vector} " + " ".join([vector] * len(arguments)) + "\n"
                   + f"%oslImport = OpFunction {vector} None %oslImportType\n"
                   + "".join(f"%oslImportArg{i} = OpFunctionParameter {vector}\n" for i in range(len(arguments)))
                   + "OpFunctionEnd\n")
    texture_type = f"\n%oslTextureType = OpTypeFunction {vector} {vector} {vector}\n" if exports else ""
    return assembly[:first_function] + texture_type + declaration + exports + assembly[first_function:]


def cook(source, destination, spirv_bin, osl_evaluator=None):
    source, destination, spirv_bin = source.resolve(), destination.resolve(), spirv_bin.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    for name in ("material.vert.spv", "material.frag.spv", "material.interface.spv"):
        (destination / name).unlink(missing_ok=True)
    shader_dir = pathlib.Path(__file__).resolve().parents[2] / "Crowny-Editor/Resources/Shaders"
    template = (shader_dir / "Pbribl.glsl").read_text(encoding="utf-8")
    stages = dict(re.findall(r"#type (vertex|fragment)\s*\n(.*?)(?=#type|\Z)", template, re.DOTALL))
    if set(stages) != {"vertex", "fragment"}:
        raise ValueError("Expected one vertex and one fragment PBR template")

    def run(tool, *args):
        subprocess.run([str(spirv_bin / (tool + (".exe" if os.name == "nt" else ""))), *map(str, args)], cwd=destination, check=True)

    for stage, suffix in (("vertex", "vert"), ("fragment", "frag")):
        text = stages[stage].replace("#version 450", "#version 450\n#extension GL_GOOGLE_include_directive : require", 1)
        if stage == "fragment":
            text = text.replace("#version 450", "#version 450\n#define CW_PROCEDURAL_BASE_COLOR 1\n#define CW_LINEAR_OUTPUT 1", 1)
            text += "\n" + source.read_text(encoding="utf-8")
        (destination / f"material.{suffix}").write_text(text, encoding="utf-8")
        run("glslangValidator", "-V", "--target-env", "spirv1.4", "-Od", f"-I{shader_dir}", f"-I{source.parent}",
            f"material.{suffix}", "-o", f"unlinked.{suffix}.spv")
    # spirv-link merges structurally identical block types, including their debug
    # names. Keep the original module as reflection metadata so distinct material
    # parameters retain their names. It is never executed by the engine.
    run("spirv-val", "--target-env", "vulkan1.3", "unlinked.frag.spv")
    fragment = "unlinked.frag.spv"
    if osl_evaluator is not None:
        run("spirv-dis", "--raw-id", fragment, "-o", "adapter.spvasm")
        assembly = import_osl((destination / "adapter.spvasm").read_text(encoding="utf-8"))
        (destination / "adapter.spvasm").write_text(assembly, encoding="utf-8")
        run("spirv-as", "--target-env", "spv1.4", "adapter.spvasm", "-o", "adapter.spv")
        run("spirv-link", "--target-env", "spv1.4", "adapter.spv", osl_evaluator.resolve(), "-o", "linked.frag.spv")
        fragment = "linked.frag.spv"
    for suffix, input_file in (("vert", "unlinked.vert.spv"), ("frag", fragment)):
        run("spirv-opt", "--target-env=vulkan1.3", "-O", input_file, "-o", f"candidate.{suffix}.spv")
        run("spirv-val", "--target-env", "vulkan1.3", f"candidate.{suffix}.spv")
    # Publish only once both stages validate.
    for suffix in ("vert", "frag"):
        (destination / f"candidate.{suffix}.spv").replace(destination / f"material.{suffix}.spv")
    (destination / "material.interface.spv").write_bytes((destination / "unlinked.frag.spv").read_bytes())
    print(f"Validated procedural material: {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--spirv-bin", required=True, type=pathlib.Path)
    parser.add_argument("--osl-evaluator", type=pathlib.Path)
    args = parser.parse_args()
    try:
        cook(args.source, args.destination, args.spirv_bin, args.osl_evaluator)
    except (subprocess.CalledProcessError, ValueError) as error:
        raise SystemExit(str(error)) from None
