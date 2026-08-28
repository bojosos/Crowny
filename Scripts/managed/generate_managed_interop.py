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
BINDINGS_OUTPUT = ROOT / "Crowny-Sharp" / "Source" / "Runtime" / "Generated" / "ManagedBindingIds.g.cs"
TYPED_HOST_OUTPUT = ROOT / "Crowny-Sharp" / "Source" / "Runtime" / "Generated" / "ManagedHostApi.g.cs"


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
    "uuid": "cw_managed_uuid",
    "font_character_info": "cw_managed_font_character_info",
    "string": "cw_managed_string_view",
    "vec2": "cw_managed_vec2",
    "vec3": "cw_managed_vec3",
    "vec4": "cw_managed_vec4",
    "quat": "cw_managed_quat",
    "mat4": "cw_managed_mat4",
}

HOST_CS_TYPES = {
    "bool": "byte",
    "f32": "float",
    "i32": "int",
    "u32": "uint",
    "uuid": "NativeUuid",
    "font_character_info": "NativeFontCharacterInfo",
    "string": "NativeStringView",
    "vec2": "NativeVec2",
    "vec3": "NativeVec3",
    "vec4": "NativeVec4",
    "quat": "NativeQuaternion",
    "mat4": "NativeMatrix4",
}

SHARP_CS_TYPES = {
    "bool": "byte",
    "f32": "float",
    "i32": "int",
    "u32": "uint",
    "uuid": "ManagedNativeUuid",
    "font_character_info": "ManagedNativeFontCharacterInfo",
    "string": "ManagedNativeStringView",
    "vec2": "ManagedNativeVec2",
    "vec3": "ManagedNativeVec3",
    "vec4": "ManagedNativeVec4",
    "quat": "ManagedNativeQuaternion",
    "mat4": "ManagedNativeMatrix4",
}

POINTER_INPUT_TYPES = {"vec2", "vec3", "vec4", "quat", "mat4"}


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


def sharp_cs_function_fields(functions: dict) -> str:
    return "\n".join(
        f"        internal {cs_function_pointer(function, SHARP_CS_TYPES, 'int')} {name};"
        for name, function in functions.items()
    )


def generate_native_abi(manifest: dict) -> str:
    entry_type, entry_method = manifest["managedEntryPoint"].split("::", 1)
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

enum cw_managed_host_binding
{{
{native_enum_members(manifest['hostBindings'], 'CW_MANAGED_BINDING_')}
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
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_get_entity_name_fn)(void* context, cw_managed_uuid entity,
                                                                         cw_managed_string_view* name);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_set_entity_name_fn)(void* context, cw_managed_uuid entity,
                                                                         cw_managed_string_view name);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_find_entity_by_name_fn)(void* context, cw_managed_string_view name,
                                                                             cw_managed_uuid* entity);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_get_entity_parent_fn)(void* context, cw_managed_uuid entity,
                                                                           cw_managed_uuid* parent);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_set_entity_parent_fn)(void* context, cw_managed_uuid entity,
                                                                           cw_managed_uuid parent);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_destroy_entity_fn)(void* context, cw_managed_uuid entity);
typedef cw_managed_status(CW_MANAGED_CALL* cw_managed_invoke_host_binding_fn)(void* context, uint32_t binding,
                                                                             cw_managed_uuid entity, cw_managed_blob input,
                                                                             cw_managed_blob* output);
{native_host_function_typedefs(manifest['hostFunctions'])}

typedef struct cw_managed_host_api
{{
    uint32_t size;
    uint32_t abi_version;
    void* context;
    cw_managed_log_fn log;
    cw_managed_get_entity_name_fn get_entity_name;
    cw_managed_set_entity_name_fn set_entity_name;
    cw_managed_find_entity_by_name_fn find_entity_by_name;
    cw_managed_get_entity_parent_fn get_entity_parent;
    cw_managed_set_entity_parent_fn set_entity_parent;
    cw_managed_destroy_entity_fn destroy_entity;
    cw_managed_invoke_host_binding_fn invoke_host_binding;
{native_host_function_fields(manifest['hostFunctions'])}
}} cw_managed_host_api;

#define CW_MANAGED_HOST_FUNCTION_LIST(X) \
{native_host_function_list(manifest['hostFunctions'])}

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
static_assert(sizeof(cw_managed_contact_point) == 32, "Managed contact ABI layout changed.");
#endif
"""


def generate_abi(manifest: dict) -> str:
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

    public enum NativeHostBinding : uint
    {{
{enum_members(manifest['hostBindings'])}
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
        public delegate* unmanaged[Cdecl]<void*, NativeUuid, NativeStringView*, NativeStatus> GetEntityName;
        public delegate* unmanaged[Cdecl]<void*, NativeUuid, NativeStringView, NativeStatus> SetEntityName;
        public delegate* unmanaged[Cdecl]<void*, NativeStringView, NativeUuid*, NativeStatus> FindEntityByName;
        public delegate* unmanaged[Cdecl]<void*, NativeUuid, NativeUuid*, NativeStatus> GetEntityParent;
        public delegate* unmanaged[Cdecl]<void*, NativeUuid, NativeUuid, NativeStatus> SetEntityParent;
        public delegate* unmanaged[Cdecl]<void*, NativeUuid, NativeStatus> DestroyEntity;
        public delegate* unmanaged[Cdecl]<void*, NativeHostBinding, NativeUuid, NativeBlob, NativeBlob*, NativeStatus> InvokeHostBinding;
{host_cs_function_fields(manifest['hostFunctions'])}

{host_cs_function_validation(manifest['hostFunctions'])}
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


def generate_binding_ids(manifest: dict) -> str:
    return f"""// <auto-generated />
namespace Crowny
{{
    internal enum ManagedBindingId : uint
    {{
{enum_members(manifest['hostBindings'])}
    }}
}}
"""


def generate_typed_host_api(manifest: dict) -> str:
    return f"""// <auto-generated />
#if !CROWNY_MONO
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Crowny
{{
    [StructLayout(LayoutKind.Sequential)]
    internal readonly unsafe struct ManagedNativeStringView
    {{
        internal readonly byte* Data;
        internal readonly uint Length;

        internal ManagedNativeStringView(byte* data, uint length) {{ Data = data; Length = length; }}
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
    internal unsafe struct ManagedNativeHostApi
    {{
        internal uint Size;
        internal uint AbiVersion;
        internal void* Context;
        internal void* Log;
        internal void* GetEntityName;
        internal void* SetEntityName;
        internal void* FindEntityByName;
        internal void* GetEntityParent;
        internal void* SetEntityParent;
        internal void* DestroyEntity;
        internal void* InvokeHostBinding;
{sharp_cs_function_fields(manifest['hostFunctions'])}
    }}
}}
#endif
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
        update(BINDINGS_OUTPUT, generate_binding_ids(manifest), arguments.check),
        update(TYPED_HOST_OUTPUT, generate_typed_host_api(manifest), arguments.check),
    )
    return 0 if all(outputs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
