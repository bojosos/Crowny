#!/usr/bin/env python3
"""Generate both sides of Crowny's versioned native ABI."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Scripts" / "managed" / "managed-interop.json"
ABI_OUTPUT = ROOT / "Crowny-Managed" / "Crowny.ManagedHost" / "Generated" / "CrownyManagedAbi.g.cs"
NATIVE_ABI_OUTPUT = ROOT / "Crowny" / "Source" / "Crowny" / "Scripting" / "Managed" / "Interop" / "CrownyManagedAbi.h"
AOT_OUTPUT = ROOT / "Crowny-Managed" / "Crowny.ManagedHost" / "Generated" / "ManagedAotRoots.g.cs"
TRIM_OUTPUT = ROOT / "Crowny-Managed" / "Crowny.ManagedHost" / "Generated" / "ILLink.Descriptors.xml"
TYPED_HOST_OUTPUT = ROOT / "Crowny-Sharp" / "Source" / "Runtime" / "Generated" / "ManagedHostApi.g.cs"
LINE_CONTINUATION = "\\"


def enum_members(values: dict[str, int]) -> str:
    return "\n".join(f"        {name} = {value}," for name, value in values.items())


def native_name(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value)
    value = re.sub(r"([A-Za-z])([0-9]+)", r"\1_\2", value)
    return value.upper()


def native_enum_members(values: dict[str, int], prefix: str) -> str:
    return "\n".join(f"    {prefix}{native_name(name)} = {value}," for name, value in values.items())


def typed_native_name(value: str) -> str:
    value = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", value)
    value = re.sub(r"([a-z])([A-Z])", r"\1_\2", value)
    return value.lower()


NATIVE_TYPES = {
    "bool": "uint8_t",
    "f32": "float",
    "i32": "int32_t",
    "u32": "uint32_t",
    "u64": "uint64_t",
    "uuid": "cw_managed_uuid",
    "font_character_info": "cw_managed_font_character_info",
    "string": "cw_managed_string_view",
    "optionalString": "cw_managed_string_view",
    "vec2": "cw_managed_vec2",
    "vec3": "cw_managed_vec3",
    "vec4": "cw_managed_vec4",
    "color": "cw_managed_vec4",
    "quat": "cw_managed_quat",
    "mat4": "cw_managed_mat4",
    "physicsFilter3D": "cw_managed_physics_filter3d",
    "bytes": "cw_managed_blob",
    "mutableBytes": "cw_managed_mutable_blob",
    "pointer": "void*",
}

HOST_CS_TYPES = {
    "bool": "byte",
    "f32": "float",
    "i32": "int",
    "u32": "uint",
    "u64": "ulong",
    "uuid": "NativeUuid",
    "font_character_info": "NativeFontCharacterInfo",
    "string": "NativeStringView",
    "optionalString": "NativeStringView",
    "vec2": "NativeVec2",
    "vec3": "NativeVec3",
    "vec4": "NativeVec4",
    "color": "NativeVec4",
    "quat": "NativeQuaternion",
    "mat4": "NativeMatrix4",
    "physicsFilter3D": "NativePhysicsFilter3D",
    "bytes": "NativeBlob",
    "mutableBytes": "NativeMutableBlob",
    "pointer": "void*",
}

SHARP_CS_TYPES = {
    "bool": "byte",
    "f32": "float",
    "i32": "int",
    "u32": "uint",
    "u64": "ulong",
    "uuid": "ManagedNativeUuid",
    "font_character_info": "ManagedNativeFontCharacterInfo",
    "string": "ManagedNativeStringView",
    "optionalString": "ManagedNativeStringView",
    "vec2": "ManagedNativeVec2",
    "vec3": "ManagedNativeVec3",
    "vec4": "ManagedNativeVec4",
    "color": "ManagedNativeVec4",
    "quat": "ManagedNativeQuaternion",
    "mat4": "ManagedNativeMatrix4",
    "physicsFilter3D": "ManagedNativePhysicsFilter3D",
    "bytes": "ManagedNativeBlob",
    "mutableBytes": "ManagedNativeMutableBlob",
    "pointer": "void*",
}

SHARP_API_TYPES = {
    "bool": "bool",
    "f32": "float",
    "i32": "int",
    "u32": "uint",
    "u64": "ulong",
    "uuid": "UUID",
    "font_character_info": "CharacterInfo",
    "string": "string",
    "optionalString": "string",
    "vec2": "Vector2",
    "vec3": "Vector3",
    "vec4": "Vector4",
    "color": "Color",
    "quat": "Quaternion",
    "mat4": "Matrix4",
    "physicsFilter3D": "PhysicsFilter3D",
    "bytes": "byte[]",
    "mutableBytes": "byte[]",
    "pointer": "IntPtr",
}

POINTER_INPUT_TYPES = {"vec2", "vec3", "vec4", "color", "quat", "mat4", "physicsFilter3D"}


def expand_host_functions(manifest: dict) -> dict:
    functions = dict(manifest.get("hostFunctions", {}))
    for module_name, module in manifest.get("hostProperties", {}).items():
        receiver = module["receiver"]
        prefix = module.get("prefix", module_name)
        for property_name, property_spec in module["properties"].items():
            if isinstance(property_spec, str):
                property_type = property_spec
                access = "readWrite"
            else:
                property_type = property_spec["type"]
                access = property_spec.get("access", "readWrite")
            if access not in {"read", "write", "readWrite"}:
                raise ValueError(f"invalid access for {module_name}.{property_name}: {access}")
            if access != "write":
                name = f"{prefix}Get{property_name}"
                if name in functions:
                    raise ValueError(f"duplicate host function: {name}")
                functions[name] = {"parameters": [receiver], "result": property_type}
            if access != "read":
                name = f"{prefix}Set{property_name}"
                if name in functions:
                    raise ValueError(f"duplicate host function: {name}")
                functions[name] = {"parameters": [receiver, {"name": "value", "type": property_type}]}
    return functions

def native_host_function_typedefs(functions: dict) -> str:
    lines: list[str] = []
    for name, function in functions.items():
        arguments = ["void* context"]
        for parameter in function.get("parameters", []):
            native_type = NATIVE_TYPES[parameter["type"]]
            if parameter["type"] in POINTER_INPUT_TYPES:
                arguments.append(f"const {native_type}* {parameter['name']}")
            else:
                arguments.append(f"{native_type} {parameter['name']}")
        if result := function.get("result"):
            arguments.append(f"{NATIVE_TYPES[result]}* result")
        lines.append(
            f"typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_{typed_native_name(name)}_fn)({', '.join(arguments)});"
        )
    return "\n".join(lines)


def native_host_function_fields(functions: dict) -> str:
    return "\n".join(
        f"    cw_managed_{typed_native_name(name)}_fn {typed_native_name(name)};" for name in functions
    )


def native_host_function_list(functions: dict) -> str:
    entries = list(functions)
    lines = []
    for index, name in enumerate(entries):
        suffix = " \\" if index + 1 != len(entries) else ""
        lines.append(f"    X({name}, {typed_native_name(name)}){suffix}")
    return "\n".join(lines)


def cs_function_pointer(function: dict, type_map: dict[str, str], status_type: str) -> str:
    arguments = ["void*"]
    for parameter in function.get("parameters", []):
        parameter_type = type_map[parameter["type"]]
        arguments.append(f"{parameter_type}*" if parameter["type"] in POINTER_INPUT_TYPES else parameter_type)
    if result := function.get("result"):
        arguments.append(f"{type_map[result]}*")
    arguments.append(status_type)
    return f"delegate* unmanaged[Cdecl]<{', '.join(arguments)}>"


def host_cs_function_fields(functions: dict) -> str:
    return "\n".join(
        f"        public {cs_function_pointer(function, HOST_CS_TYPES, 'NativeStatus')} {name};"
        for name, function in functions.items()
    )


def host_cs_function_validation(functions: dict) -> str:
    conditions = " &&\n                   ".join(f"{name} != null" for name in functions)
    return f"        public readonly bool HasCompleteBindings() =>\n            {conditions};"


def sharp_transport_function_fields(functions: dict) -> str:
    return "\n".join(f"        internal IntPtr {name};" for name in functions)


def sharp_transport_signature(function: dict, include_context: bool = False) -> tuple[str, str]:
    declarations = ["void* context"] if include_context else []
    arguments = ["context"] if include_context else []
    for parameter in function.get("parameters", []):
        parameter_type = SHARP_CS_TYPES[parameter["type"]]
        if parameter["type"] in POINTER_INPUT_TYPES:
            parameter_type += "*"
        declarations.append(f"{parameter_type} {parameter['name']}")
        arguments.append(parameter["name"])
    if result := function.get("result"):
        declarations.append(f"{SHARP_CS_TYPES[result]}* result")
        arguments.append("result")
    return ", ".join(declarations), ", ".join(arguments)


def sharp_transport_delegate_types(functions: dict) -> dict[str, str]:
    types_by_signature: dict[str, str] = {}
    result: dict[str, str] = {}
    for name, function in functions.items():
        signature, _ = sharp_transport_signature(function, include_context=True)
        delegate_type = types_by_signature.setdefault(signature, f"ManagedHostCall{len(types_by_signature)}")
        result[name] = delegate_type
    return result


def sharp_transport_delegates(functions: dict) -> str:
    delegate_types = sharp_transport_delegate_types(functions)
    signatures: dict[str, str] = {}
    for name, function in functions.items():
        signature, _ = sharp_transport_signature(function, include_context=True)
        signatures.setdefault(delegate_types[name], signature)
    declarations = []
    for delegate_type, signature in signatures.items():
        declarations.extend(
            [
                "        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]",
                f"        private delegate int {delegate_type}({signature});",
            ]
        )
    declarations.append("")
    declarations.extend(
        f"        private static {delegate_types[name]} {name}Callback;" for name in functions
    )
    return "\n".join(declarations)


def sharp_transport_bindings(functions: dict) -> str:
    delegate_types = sharp_transport_delegate_types(functions)
    return "\n".join(
        f"            {name}Callback = Marshal.GetDelegateForFunctionPointer<{delegate_types[name]}>(value.{name});"
        for name in functions
    )


def sharp_transport_validation(functions: dict) -> str:
    conditions = " &&\n                   ".join(f"value.{name} != IntPtr.Zero" for name in functions)
    return f"            bool complete =\n                {conditions};"


def sharp_transport_methods(functions: dict) -> str:
    methods = []
    for name, function in functions.items():
        signature, arguments = sharp_transport_signature(function)
        call_arguments = f", {arguments}" if arguments else ""
        methods.append(
            f"        internal static int {name}({signature}) => {name}Callback(api.Context.ToPointer(){call_arguments});"
        )
    return "\n".join(methods)


def sharp_native_parameter(parameter: dict) -> tuple[list[str], str]:
    name = parameter["name"]
    native_name = f"native{name[0].upper()}{name[1:]}"
    parameter_type = parameter["type"]
    if parameter_type == "bool":
        return [], f"{name} ? (byte)1 : (byte)0"
    if parameter_type in {"f32", "i32", "u32", "u64"}:
        return [], name
    if parameter_type == "pointer":
        return [], f"{name}.ToPointer()"
    if parameter_type == "uuid":
        return [], f"EncodeUuid({name})"
    if parameter_type == "string":
        return [], native_name
    if parameter_type in {"bytes", "mutableBytes"}:
        return [], native_name
    if parameter_type == "vec2":
        return [f"            ManagedNativeVec2 {native_name} = new ManagedNativeVec2 {{ X = {name}.x, Y = {name}.y }};"], f"&{native_name}"
    if parameter_type == "vec3":
        return [f"            ManagedNativeVec3 {native_name} = new ManagedNativeVec3 {{ X = {name}.x, Y = {name}.y, Z = {name}.z }};"], f"&{native_name}"
    if parameter_type == "vec4":
        return [f"            ManagedNativeVec4 {native_name} = new ManagedNativeVec4 {{ X = {name}.x, Y = {name}.y, Z = {name}.z, W = {name}.w }};"], f"&{native_name}"
    if parameter_type == "color":
        return [f"            ManagedNativeVec4 {native_name} = new ManagedNativeVec4 {{ X = {name}.r, Y = {name}.g, Z = {name}.b, W = {name}.a }};"], f"&{native_name}"
    if parameter_type == "quat":
        return [f"            ManagedNativeQuaternion {native_name} = new ManagedNativeQuaternion {{ X = {name}.x, Y = {name}.y, Z = {name}.z, W = {name}.w }};"], f"&{native_name}"
    if parameter_type == "mat4":
        return [f"            ManagedNativeMatrix4 {native_name} = EncodeMatrix({name});"], f"&{native_name}"
    if parameter_type == "physicsFilter3D":
        return [f"            ManagedNativePhysicsFilter3D {native_name} = new ManagedNativePhysicsFilter3D {{ Layer = {name}.Layer, Mask = {name}.Mask, Group = {name}.Group }};"], f"&{native_name}"
    raise ValueError(f"unsupported managed parameter type: {parameter_type}")


def sharp_result_declaration(result_type: str) -> tuple[str, str]:
    native_type = SHARP_CS_TYPES[result_type]
    declaration = f"            {native_type} result = default;"
    if result_type == "bool":
        return declaration, "result != 0"
    if result_type in {"f32", "i32", "u32", "u64"}:
        return declaration, "result"
    if result_type == "uuid":
        return declaration, "DecodeUuid(result)"
    if result_type == "font_character_info":
        return declaration, "DecodeFontCharacterInfo(result)"
    if result_type == "vec2":
        return declaration, "new Vector2(result.X, result.Y)"
    if result_type == "vec3":
        return declaration, "new Vector3(result.X, result.Y, result.Z)"
    if result_type == "vec4":
        return declaration, "new Vector4(result.X, result.Y, result.Z, result.W)"
    if result_type == "color":
        return declaration, "new Color(result.X, result.Y, result.Z, result.W)"
    if result_type == "quat":
        return declaration, "new Quaternion(result.X, result.Y, result.Z, result.W)"
    if result_type == "mat4":
        return declaration, "DecodeMatrix(result)"
    if result_type == "physicsFilter3D":
        return declaration, "new PhysicsFilter3D(result.Layer, result.Mask, result.Group)"
    if result_type == "string":
        return declaration, "DecodeString(result)"
    if result_type == "optionalString":
        return declaration, "DecodeOptionalString(result)"
    if result_type == "bytes":
        return declaration, "DecodeBytes(result)"
    raise ValueError(f"unsupported managed result type: {result_type}")


def sharp_cs_function_wrappers(functions: dict) -> str:
    methods: list[str] = []
    for function_name, function in functions.items():
        parameters = function.get("parameters", [])
        result_type = function.get("result")
        return_type = SHARP_API_TYPES[result_type] if result_type else "void"
        signature = ", ".join(f"{SHARP_API_TYPES[value['type']]} {value['name']}" for value in parameters)
        body: list[str] = [
            f"        internal static {return_type} {function_name}({signature})",
            "        {",
            "            EnsureHostBindings();",
        ]
        call_arguments = []
        for parameter in parameters:
            declarations, argument = sharp_native_parameter(parameter)
            body.extend(declarations)
            call_arguments.append(argument)
        result_expression = None
        if result_type:
            declaration, result_expression = sharp_result_declaration(result_type)
            body.append(declaration)
            call_arguments.append("&result")

        view_parameters = [value for value in parameters if value["type"] in {"string", "bytes", "mutableBytes"}]
        for parameter in view_parameters:
            name = parameter["name"]
            title = f"{name[0].upper()}{name[1:]}"
            if parameter["type"] == "string":
                body.append(f"            byte[] encoded{title} = Encoding.UTF8.GetBytes({name} ?? string.Empty);")
            else:
                body.append(f"            byte[] encoded{title} = {name} ?? Array.Empty<byte>();")
        indent = "            "
        for parameter in view_parameters:
            name = parameter["name"]
            title = f"{name[0].upper()}{name[1:]}"
            body.append(f"{indent}fixed (byte* {name}Bytes = encoded{title})")
            body.append(f"{indent}{{")
            indent += "    "
            if parameter["type"] == "string":
                body.append(
                    f"{indent}ManagedNativeStringView native{title} = new ManagedNativeStringView({name}Bytes, (uint)encoded{title}.Length);"
                )
            elif parameter["type"] == "bytes":
                body.append(f"{indent}ManagedNativeBlob native{title} = new ManagedNativeBlob({name}Bytes, (ulong)encoded{title}.Length);")
            else:
                body.append(f"{indent}ManagedNativeMutableBlob native{title} = new ManagedNativeMutableBlob({name}Bytes, (ulong)encoded{title}.Length);")

        arguments = ", ".join(call_arguments)
        body.append(f"{indent}EnsureStatus(ManagedHostTransport.{function_name}({arguments}), \"{function_name}\");")
        for _ in reversed(view_parameters):
            indent = indent[:-4]
            body.append(f"{indent}}}")
        if result_expression:
            body.append(f"            return {result_expression};")
        body.append("        }")
        methods.append("\n".join(body))
    return "\n\n".join(methods)


def generate_native_abi(manifest: dict) -> str:
    entry_type, entry_method = manifest["managedEntryPoint"].split("::", 1)
    functions = expand_host_functions(manifest)
    return f"""#pragma once
// <auto-generated />

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define CW_MANAGED_CALL __cdecl
#define CW_MANAGED_EXPORT __declspec(dllexport)
#else
#define CW_MANAGED_CALL
#define CW_MANAGED_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {{
#endif

#define CW_MANAGED_ABI_VERSION {manifest['abiVersion']}u
#define CW_MANAGED_BOOTSTRAP_TYPE "{entry_type}"
#define CW_MANAGED_BOOTSTRAP_METHOD "{entry_method}"

typedef int32_t cw_managed_status;
typedef uint64_t cw_managed_instance;

enum cw_managed_status_code
{{
{native_enum_members(manifest['statusCodes'], 'CW_MANAGED_STATUS_')}
}};

enum cw_managed_event_kind
{{
{native_enum_members(manifest['eventKinds'], 'CW_MANAGED_EVENT_')}
}};

typedef struct cw_managed_string_view
{{
    const uint8_t* data;
    uint32_t length;
}} cw_managed_string_view;

typedef struct cw_managed_blob
{{
    const uint8_t* data;
    uint64_t length;
}} cw_managed_blob;

typedef struct cw_managed_mutable_blob
{{
    uint8_t* data;
    uint64_t length;
}} cw_managed_mutable_blob;

typedef struct cw_managed_uuid
{{
    uint8_t bytes[16];
}} cw_managed_uuid;

typedef struct cw_managed_font_character_info
{{
    cw_managed_uuid source_font;
    uint32_t requested_code_point;
    uint32_t resolved_code_point;
    int32_t glyph_index;
    uint32_t reserved;
    double advance;
    double plane_left;
    double plane_bottom;
    double plane_right;
    double plane_top;
    double atlas_left;
    double atlas_bottom;
    double atlas_right;
    double atlas_top;
    uint8_t whitespace;
    uint8_t valid;
    uint8_t reserved_tail[6];
}} cw_managed_font_character_info;

typedef struct cw_managed_vec2
{{
    float x;
    float y;
}} cw_managed_vec2;

typedef struct cw_managed_vec3
{{
    float x;
    float y;
    float z;
}} cw_managed_vec3;

typedef struct cw_managed_vec4
{{
    float x;
    float y;
    float z;
    float w;
}} cw_managed_vec4;

typedef struct cw_managed_quat
{{
    float x;
    float y;
    float z;
    float w;
}} cw_managed_quat;

typedef struct cw_managed_mat4
{{
    float values[16];
}} cw_managed_mat4;

typedef struct cw_managed_physics_filter3d
{{
    uint32_t layer;
    uint32_t mask;
    int32_t group;
}} cw_managed_physics_filter3d;

typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_write_blob_fn)(void* context, const uint8_t* data, uint64_t length);

typedef struct cw_managed_blob_writer
{{
    uint32_t size;
    void* context;
    cw_managed_write_blob_fn write;
}} cw_managed_blob_writer;

typedef struct cw_managed_contact_point
{{
    float position[3];
    float normal[3];
    float separation;
    float impulse;
}} cw_managed_contact_point;

typedef struct cw_managed_event
{{
    uint32_t size;
    uint32_t kind;
    float delta_time;
    cw_managed_uuid other_entity;
    float relative_velocity[3];
    cw_managed_blob payload; /* Packed cw_managed_contact_point values. */
}} cw_managed_event;

typedef void(CW_MANAGED_CALL* cw_managed_log_fn)(void* context, uint32_t severity, cw_managed_string_view code,
                                                 cw_managed_string_view message, cw_managed_string_view stack);
{native_host_function_typedefs(functions)}

typedef struct cw_managed_host_api
{{
    uint32_t size;
    uint32_t abi_version;
    void* context;
    cw_managed_log_fn log;
{native_host_function_fields(functions)}
}} cw_managed_host_api;

#define CW_MANAGED_HOST_FUNCTION_LIST(X) {LINE_CONTINUATION}
{native_host_function_list(functions)}

typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_initialize_fn)(const cw_managed_host_api* host);
typedef void(CW_MANAGED_CALL* cw_managed_shutdown_fn)(void);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_load_program_fn)(cw_managed_string_view assembly_path, uint64_t generation);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_unload_program_fn)(void);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_get_catalog_fn)(cw_managed_blob_writer* output);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_create_script_fn)(cw_managed_string_view assembly_name,
                                                                       cw_managed_string_view type_namespace,
                                                                       cw_managed_string_view type_name,
                                                                       cw_managed_uuid entity, cw_managed_blob initial_state,
                                                                       cw_managed_instance* instance);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_destroy_script_fn)(cw_managed_instance instance);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_dispatch_fn)(cw_managed_instance instance,
                                                                  const cw_managed_event* event_data);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_notify_scene_event_fn)(uint32_t event_type,
                                                                            cw_managed_uuid scene,
                                                                            uint32_t execution_state);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_capture_state_fn)(cw_managed_instance instance,
                                                                       cw_managed_blob_writer* output);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_apply_state_fn)(cw_managed_instance instance, cw_managed_blob state);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_collect_diagnostics_fn)(cw_managed_blob_writer* output);

typedef struct cw_managed_program_api
{{
    uint32_t size;
    uint32_t abi_version;
    cw_managed_initialize_fn initialize;
    cw_managed_shutdown_fn shutdown;
    cw_managed_load_program_fn load_program;
    cw_managed_unload_program_fn unload_program;
    cw_managed_get_catalog_fn get_catalog;
    cw_managed_create_script_fn create_script;
    cw_managed_destroy_script_fn destroy_script;
    cw_managed_dispatch_fn dispatch;
    cw_managed_notify_scene_event_fn notify_scene_event;
    cw_managed_capture_state_fn capture_state;
    cw_managed_apply_state_fn apply_state;
    cw_managed_collect_diagnostics_fn collect_diagnostics;
}} cw_managed_program_api;

typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_get_api_fn)(cw_managed_program_api* api, uint32_t api_size);

#ifdef __cplusplus
}}

static_assert(sizeof(cw_managed_uuid) == 16, "Managed UUID ABI layout changed.");
static_assert(sizeof(cw_managed_font_character_info) == 112, "Managed font character ABI layout changed.");
static_assert(sizeof(cw_managed_vec2) == 8, "Managed Vector2 ABI layout changed.");
static_assert(sizeof(cw_managed_vec3) == 12, "Managed Vector3 ABI layout changed.");
static_assert(sizeof(cw_managed_vec4) == 16, "Managed Vector4 ABI layout changed.");
static_assert(sizeof(cw_managed_quat) == 16, "Managed quaternion ABI layout changed.");
static_assert(sizeof(cw_managed_mat4) == 64, "Managed Matrix4 ABI layout changed.");
static_assert(sizeof(cw_managed_physics_filter3d) == 12, "Managed PhysicsFilter3D ABI layout changed.");
static_assert(sizeof(cw_managed_contact_point) == 32, "Managed contact ABI layout changed.");
#endif
"""


def generate_abi(manifest: dict) -> str:
    functions = expand_host_functions(manifest)
    return f"""// <auto-generated />
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny.ManagedHost.Interop
{{
    public enum NativeStatus : int
    {{
{enum_members(manifest['statusCodes'])}
    }}

    public enum NativeEventKind : uint
    {{
{enum_members(manifest['eventKinds'])}
    }}

    [StructLayout(LayoutKind.Sequential)]
    public readonly unsafe struct NativeStringView
    {{
        public readonly byte* Data;
        public readonly uint Length;

        public NativeStringView(byte* data, uint length) {{ Data = data; Length = length; }}
    }}

    [StructLayout(LayoutKind.Sequential)]
    public readonly unsafe struct NativeBlob
    {{
        public readonly byte* Data;
        public readonly ulong Length;

        public NativeBlob(byte* data, ulong length) {{ Data = data; Length = length; }}
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeUuid
    {{
        public fixed byte Bytes[16];
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeFontCharacterInfo
    {{
        public NativeUuid SourceFont;
        public uint RequestedCodePoint;
        public uint ResolvedCodePoint;
        public int GlyphIndex;
        public uint Reserved;
        public double Advance;
        public double PlaneLeft;
        public double PlaneBottom;
        public double PlaneRight;
        public double PlaneTop;
        public double AtlasLeft;
        public double AtlasBottom;
        public double AtlasRight;
        public double AtlasTop;
        public byte Whitespace;
        public byte Valid;
        public byte ReservedTail0;
        public byte ReservedTail1;
        public byte ReservedTail2;
        public byte ReservedTail3;
        public byte ReservedTail4;
        public byte ReservedTail5;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVec2
    {{
        public float X;
        public float Y;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVec3
    {{
        public float X;
        public float Y;
        public float Z;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeVec4
    {{
        public float X;
        public float Y;
        public float Z;
        public float W;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeQuaternion
    {{
        public float X;
        public float Y;
        public float Z;
        public float W;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeMatrix4
    {{
        public fixed float Values[16];
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativePhysicsFilter3D
    {{
        public uint Layer;
        public uint Mask;
        public int Group;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeBlobWriter
    {{
        public uint Size;
        public void* Context;
        public delegate* unmanaged[Cdecl]<void*, byte*, ulong, NativeStatus> Write;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeContactPoint
    {{
        public float PositionX;
        public float PositionY;
        public float PositionZ;
        public float NormalX;
        public float NormalY;
        public float NormalZ;
        public float Separation;
        public float Impulse;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeEvent
    {{
        public uint Size;
        public NativeEventKind Kind;
        public float DeltaTime;
        public NativeUuid OtherEntity;
        public fixed float RelativeVelocity[3];
        public NativeBlob Payload;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeHostApi
    {{
        public uint Size;
        public uint AbiVersion;
        public void* Context;
        public delegate* unmanaged[Cdecl]<void*, uint, NativeStringView, NativeStringView, NativeStringView, void> Log;
{host_cs_function_fields(functions)}

{host_cs_function_validation(functions)}
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeMutableBlob
    {{
        public byte* Data;
        public ulong Length;
    }}

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct NativeProgramApi
    {{
        public uint Size;
        public uint AbiVersion;
        public delegate* unmanaged[Cdecl]<NativeHostApi*, NativeStatus> Initialize;
        public delegate* unmanaged[Cdecl]<void> Shutdown;
        public delegate* unmanaged[Cdecl]<NativeStringView, ulong, NativeStatus> LoadProgram;
        public delegate* unmanaged[Cdecl]<NativeStatus> UnloadProgram;
        public delegate* unmanaged[Cdecl]<NativeBlobWriter*, NativeStatus> GetCatalog;
        public delegate* unmanaged[Cdecl]<NativeStringView, NativeStringView, NativeStringView, NativeUuid, NativeBlob, ulong*, NativeStatus> CreateScript;
        public delegate* unmanaged[Cdecl]<ulong, NativeStatus> DestroyScript;
        public delegate* unmanaged[Cdecl]<ulong, NativeEvent*, NativeStatus> Dispatch;
        public delegate* unmanaged[Cdecl]<uint, NativeUuid, uint, NativeStatus> NotifySceneEvent;
        public delegate* unmanaged[Cdecl]<ulong, NativeBlobWriter*, NativeStatus> CaptureState;
        public delegate* unmanaged[Cdecl]<ulong, NativeBlob, NativeStatus> ApplyState;
        public delegate* unmanaged[Cdecl]<NativeBlobWriter*, NativeStatus> CollectDiagnostics;
    }}

    public static class NativeAbi
    {{
        public const uint Version = {manifest['abiVersion']};
        public const string EntryPoint = "{manifest['managedEntryPoint']}";
    }}
}}
"""


def generate_aot_roots(manifest: dict) -> str:
    attributes = "\n".join(
        "        [DynamicDependency(DynamicallyAccessedMemberTypes.All, typeof(" + name + "))]" for name in manifest["aotRoots"]
    )
    return f"""// <auto-generated />
using System.Diagnostics.CodeAnalysis;

namespace Crowny.ManagedHost
{{
    internal static class ManagedAotRoots
    {{
{attributes}
        internal static void Preserve() {{ }}
    }}
}}
"""


def generate_typed_host_api(manifest: dict) -> str:
    functions = expand_host_functions(manifest)
    return f"""// <auto-generated />
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Crowny
{{
    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeStringView
    {{
        internal byte* Data;
        internal uint Length;

        internal ManagedNativeStringView(byte* data, uint length) {{ Data = data; Length = length; }}
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeBlob
    {{
        internal byte* Data;
        internal ulong Length;

        internal ManagedNativeBlob(byte* data, ulong length) {{ Data = data; Length = length; }}
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeMutableBlob
    {{
        internal byte* Data;
        internal ulong Length;

        internal ManagedNativeMutableBlob(byte* data, ulong length) {{ Data = data; Length = length; }}
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeUuid
    {{
        internal fixed byte Bytes[16];
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativeFontCharacterInfo
    {{
        internal ManagedNativeUuid SourceFont;
        internal uint RequestedCodePoint;
        internal uint ResolvedCodePoint;
        internal int GlyphIndex;
        internal uint Reserved;
        internal double Advance;
        internal double PlaneLeft;
        internal double PlaneBottom;
        internal double PlaneRight;
        internal double PlaneTop;
        internal double AtlasLeft;
        internal double AtlasBottom;
        internal double AtlasRight;
        internal double AtlasTop;
        internal byte Whitespace;
        internal byte Valid;
        internal byte ReservedTail0;
        internal byte ReservedTail1;
        internal byte ReservedTail2;
        internal byte ReservedTail3;
        internal byte ReservedTail4;
        internal byte ReservedTail5;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativeVec2
    {{
        internal float X;
        internal float Y;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativeVec3
    {{
        internal float X;
        internal float Y;
        internal float Z;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativeVec4
    {{
        internal float X;
        internal float Y;
        internal float Z;
        internal float W;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativeQuaternion
    {{
        internal float X;
        internal float Y;
        internal float Z;
        internal float W;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeMatrix4
    {{
        internal fixed float Values[16];
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal struct ManagedNativePhysicsFilter3D
    {{
        internal uint Layer;
        internal uint Mask;
        internal int Group;
    }}

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct ManagedNativeHostApi
    {{
        internal uint Size;
        internal uint AbiVersion;
        internal IntPtr Context;
        internal IntPtr Log;
{sharp_transport_function_fields(functions)}
    }}

    internal static unsafe class ManagedHostTransport
    {{
        private static ManagedNativeHostApi api;

        internal static bool IsInitialized
        {{
            get {{ return api.Context != IntPtr.Zero; }}
        }}

        internal static void SetApi(ManagedNativeHostApi value)
        {{
            if (value.Context == IntPtr.Zero)
            {{
                api = value;
                return;
            }}
            if (value.AbiVersion != {manifest['abiVersion']} || value.Size < (uint)Marshal.SizeOf(typeof(ManagedNativeHostApi)))
                throw new InvalidOperationException("The native host uses an incompatible managed scripting ABI.");
{sharp_transport_validation(functions)}
            if (!complete)
                throw new InvalidOperationException("The native host did not provide every managed binding.");
{sharp_transport_bindings(functions)}
            api = value;
        }}

{sharp_transport_delegates(functions)}

{sharp_transport_methods(functions)}
    }}

    internal static unsafe partial class ManagedRuntimeContext
    {{
{sharp_cs_function_wrappers(functions)}
    }}
}}
"""


def generate_trim_roots(manifest: dict) -> str:
    types = "\n".join(f'    <type fullname="{name}" preserve="all" />' for name in manifest["aotRoots"])
    return f"""<?xml version="1.0" encoding="utf-8"?>
<linker>
  <assembly fullname="CrownySharp">
{types}
  </assembly>
</linker>
"""


def update(path: Path, content: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.exists() else None
    if current == content:
        return True
    if check:
        print(f"stale generated file: {path.relative_to(ROOT)}")
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    print(f"generated {path.relative_to(ROOT)}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    outputs = (
        update(NATIVE_ABI_OUTPUT, generate_native_abi(manifest), arguments.check),
        update(ABI_OUTPUT, generate_abi(manifest), arguments.check),
        update(AOT_OUTPUT, generate_aot_roots(manifest), arguments.check),
        update(TRIM_OUTPUT, generate_trim_roots(manifest), arguments.check),
        update(TYPED_HOST_OUTPUT, generate_typed_host_api(manifest), arguments.check),
    )
    return 0 if all(outputs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
