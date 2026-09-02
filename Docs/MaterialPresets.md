# Material presets

A material preset is a named set of parameter values that can be applied to any material whose shader exposes those parameters. Presets never store texture assignments, so applying one restyles a material while keeping its maps.

Presets are plain YAML files with the `.cwpreset` extension. The engine ships its presets under `Crowny-Editor/Resources/Presets/<Group>/<Name>.cwpreset` and packs them into `Builtin.cwpack`; projects keep their own anywhere below the asset folder.

## File format

```yaml
# Crowny Material Preset
Version: 1
Name: Classic
Target: Toon
Parameters:
  - {Name: tint, Type: Color, Value: [1, 1, 1, 1]}
  - {Name: bands, Type: Float, Value: 3}
  - {Name: toonPatternMapping, Type: Int, Value: 0}
```

| Key | Meaning |
| --- | --- |
| `Version` | Format version, currently `1`. Newer versions are rejected. |
| `Name` | Display name. Defaults to the file stem when omitted. |
| `Target` | Material model (`Standard`, `Unlit`, `Toon`) or shader name the preset is meant for. Empty matches any material. Matching is case-insensitive. |
| `Parameters` | Sequence of `Name`, `Type`, `Value` entries. |

Supported `Type` values are `Float`, `Float2`, `Float3`, `Color` (four components; `Float4` is accepted as an alias), `Int`, and `Bool`. Matrices and byte vectors cannot be preset.

Applying a preset is all-or-nothing: `Material::ApplyPreset` validates that every parameter exists in the shader's reflected layout with the same type and leaves the material untouched otherwise. The inspector reports the offending parameter.

## Built-in presets

| Name | Look |
| --- | --- |
| `Toon/Classic` | Three hard bands, black outlines, neutral specular and rim. |
| `Toon/Soft` | Four smooth bands, tinted shadows, warm rim light. |
| `Toon/Hatched` | Dark ink outlines with a procedural hatching pattern in the shadows. |

`MaterialPresetLibrary::LoadBuiltIn("Toon/Classic")` loads and caches a built-in preset. The legacy `ToonMaterialPreset` enum and `Material::ApplyToonPreset` remain as a compatibility shim over these files, so scripts calling `MaterialApplyToonPreset` keep working. Editing a `.cwpreset` changes the look without recompiling the engine; call `MaterialPresetLibrary::ClearCache()` to pick up edits within a running session.

Source files are preferred over the packed copies during development, exactly like shaders. When adding a built-in preset, run `Scripts/pack-builtins.py` (or the PowerShell variant) so `Builtin.cwpack` contains it; the scripts include `Resources/Presets/*/*.cwpreset`.

## Project presets

Any `.cwpreset` below the project's asset folder is imported as a `MaterialPreset` asset by `MaterialPresetImporter`. In the material inspector:

- **Apply preset...** lists built-in and project presets compatible with the material's model or shader, with search.
- **Save as...** captures the material's current values into `<Material> Preset.cwpreset` next to the material file and imports it. Edit the `Target` line if the preset should also apply to other shaders.

`ProjectLibrary::SaveEntry` writes presets as YAML so they stay diffable and hand-editable.

## Shader selection

The same inspector section has a **Shader** picker that lists the engine's built-in surface shaders and every shader imported into the project. Built-in shaders get stable identifiers from `BuiltInShaderCatalog` (a name-based UUID derived from `Resources/Shaders/<Name>.asset`), so a material saved with a built-in shader resolves the same shader after the editor restarts. Internal engine shaders (compute, depth, post-process) are hidden unless "Show internal shaders" is enabled because the mesh renderer cannot draw materials that use them.
