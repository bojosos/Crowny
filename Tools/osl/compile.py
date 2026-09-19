"""Compile an OSL color output to a live Vulkan material package."""
import argparse
import json
import os
import pathlib
import shlex
import subprocess
import sys


def validate_texture_inputs(path):
    """Texture inputs are asset handles, never general runtime OSL strings.

    Check the unoptimized OSO instructions before OSL can constant-fold string
    operations on the compiler's resource tokens. OSO operands are symbol names.
    """
    texture_inputs = set()
    code = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        words = shlex.split(line.split("%", 1)[0])
        if not words:
            continue
        if words[0] == "param" and words[1] == "string":
            texture_inputs.add(words[2])
        if words[0] == "code":
            code = True
            continue
        if code:
            for index, operand in enumerate(words[1:]):
                if operand in texture_inputs and not (words[0] == "texture" and index == 1):
                    raise ValueError(f"OSL texture input '{operand}' may only be used as the filename argument of texture()")


def material_adapter(parameters, textures):
    types = {"float": "float", "int": "int", "color": "vec3", "point": "vec3", "vector": "vec3", "normal": "vec3"}
    uniforms, arguments, values, metadata = [], ["vec4 uvTime", "vec4 gradients"], ["vec4(context.uv, context.time, 0)",
        "vec4(context.uvDx.x, context.uvDy.x, context.uvDx.y, context.uvDy.y)"], []
    for index, parameter in enumerate(parameters):
        name, kind = "osl_" + parameter["Name"], parameter["Type"]
        uniforms.append(f"layout(offset = {index * 16}) {types[kind]} {name};")
        arguments.append(f"vec4 input{index}")
        if kind == "int":
            values.append(f"vec4(intBitsToFloat({name}), 0, 0, 0)")
        elif kind == "float":
            values.append(f"vec4({name}, 0, 0, 0)")
        else:
            values.append(f"vec4({name}, 0)")
        metadata.append({**parameter, "Name": name, "Block": "OslInputs", "Offset": index * 16})
    block = "layout(std140, set = 0, binding = 14) uniform OslInputs {\n" + "\n".join(uniforms) + "\n};\n" if uniforms else ""
    texture_code = ""
    texture_metadata = []
    keep_alive = ""
    for index, texture in enumerate(textures):
        name = "osl_" + texture["Name"]
        texture_code += (f"layout(set = 0, binding = {15 + index}) uniform sampler2D {name};\n"
                         f"vec4 crownyOslTexture{index}(vec4 uv, vec4 gradients) {{\n"
                         f"return textureGrad({name}, uv.xy, gradients.xy, gradients.zw);\n}}\n")
        # The unlinked module supplies reflection and exported implementations.
        # Keep these functions reachable until the evaluator stub is replaced.
        keep_alive += f" + crownyOslTexture{index}(uvTime, gradients)"
        texture_metadata.append({**texture, "Name": name, "Set": 0, "Binding": 15 + index})
    stub = f"vec4 crownyOslEvaluate({', '.join(arguments)}) {{ return uvTime + gradients{keep_alive}; }}\n"
    adapter = "vec4 cwEvaluateProceduralTexture(CwTextureContext context) {\nreturn crownyOslEvaluate(" + ", ".join(values) + ");\n}\n"
    return block + texture_code + stub + adapter, metadata, texture_metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--output", default="Cout")
    parser.add_argument("--compiler", required=True, type=pathlib.Path)
    parser.add_argument("--shadeops", required=True, type=pathlib.Path)
    parser.add_argument("--oslc", default="oslc")
    parser.add_argument("--llc", default="llc")
    parser.add_argument("--spirv-bin", required=True, type=pathlib.Path)
    parser.add_argument("--test-reference-generator", type=pathlib.Path,
                        help="Opt in to CPU reference fixtures and a compute test module; never needed for material import")
    args = parser.parse_args()
    source, destination = args.source.resolve(), args.destination.resolve()
    compiler, shadeops = args.compiler.resolve(), args.shadeops.resolve()
    spirv_bin = args.spirv_bin.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    # A failed compilation must not leave a previous shader looking current.
    for name in ("evaluate.spv", "material.vert.spv", "material.frag.spv", "material.interface.spv", "material.parameters.json",
                 "samples.bin", "reference.bin", "surface-reference.bin", "surface-parameters.bin", "textures.json"):
        (destination / name).unlink(missing_ok=True)

    def run(*command):
        subprocess.run([str(arg) for arg in command], cwd=destination, check=True)

    run(args.oslc, "-o", "shader.oso", source)
    validate_texture_inputs(destination / "shader.oso")
    run(compiler, "shader.oso", args.output, shadeops)
    textures = json.loads((destination / "textures.json").read_text())
    run(args.llc, "-O0", "-mtriple=spirv-unknown-unknown", "-filetype=obj", "evaluator.ll", "-o", "evaluator.spv")
    wrapper = pathlib.Path(__file__).with_name("Evaluate.spvasm").resolve()
    if args.test_reference_generator and not textures:
        suffix = ".exe" if os.name == "nt" else ""
        run(spirv_bin / ("spirv-as" + suffix), "--target-env", "spv1.4", wrapper, "-o", "wrapper.spv")
        run(spirv_bin / ("spirv-link" + suffix), "--target-env", "spv1.4", "wrapper.spv", "evaluator.spv", "-o", "linked.spv")
        run(spirv_bin / ("spirv-opt" + suffix), "--target-env=vulkan1.3", "-O", "linked.spv", "-o", "candidate.spv")
        run(spirv_bin / ("spirv-val" + suffix), "--target-env", "vulkan1.3", "candidate.spv")
    tool_dir = pathlib.Path(__file__).resolve().parent
    adapter, metadata, texture_metadata = material_adapter(json.loads((destination / "parameters.json").read_text()), textures)
    (destination / "MaterialTexture.glslinc").write_text(adapter, encoding="utf-8")
    run(sys.executable, tool_dir.parent / "procedural/compile.py", destination / "MaterialTexture.glslinc", destination,
        "--spirv-bin", spirv_bin, "--osl-evaluator", destination / "evaluator.spv")
    (destination / "material.parameters.json").write_text(json.dumps({"Version": 1, "Parameters": metadata,
        "Textures": texture_metadata}, indent=2), encoding="utf-8")
    if args.test_reference_generator:
        run(args.test_reference_generator.resolve(), "shader.oso", args.output)
    if args.test_reference_generator and not textures:
        (destination / "candidate.spv").replace(destination / "evaluate.spv")
    print(f"Validated OSL Vulkan package: {destination}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from None
    except ValueError as error:
        raise SystemExit(str(error)) from None
