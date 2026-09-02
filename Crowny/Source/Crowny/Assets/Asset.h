#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Uuid.h"

#include <cereal/types/polymorphic.hpp>

namespace Crowny
{

    class ImportOptions;

    enum class AssetType
    {
        None,
        AudioClip,
        Texture,
        Shader,
        Material,
        Mesh,
        ScriptCode,
        PhysicsMaterial2D,
        PhysicsMaterial,
        MeshSource,
        PhysicsMesh,
        PlainText,
        Font,
        Scene,
        NodeGraph,
        EnvironmentMap,
        Prefab,
        AudioMixer,
        AnimationClip,
        MaterialPreset
    };

    static constexpr uint32_t ASSET_FILE_MAGIC = 0x43574E59; // "CWNY"

    struct AssetFileHeader
    {
        uint32_t Magic = 0;
        uint32_t Version = 0;
        AssetType Type = AssetType::None;
        int64_t SourceTimestamp = 0;    // last_write_time of source file when compiled (epoch seconds)
        int64_t CompileTimestamp = 0;   // when this .asset was created (epoch seconds)
        uint64_t SourceContentHash = 0; // hash of source file content — definitive staleness check
    };

    // Per-asset-type format versions
    // Version 2 stores Basis Universal KTX2 source payloads without a GPU readback.
    // Version 1 raw subresource payloads remain readable.
    static constexpr uint32_t TEXTURE_FORMAT_VERSION = 2;
    // Version 4 switches SourceContentHash from FNV-1a to CityHash64.
    // Version 5 targets SPIR-V 1.5 so fragment discard remains portable to OpenGL.
    // Version 6 persists descriptor-array reflection for bindless material resources.
    static constexpr uint32_t SHADER_FORMAT_VERSION = 6;
    // Version 3 persists the optional explicit alpha rendering mode. Version 4
    // migrates the legacy toon hull thickness to its independent silhouette width.
    static constexpr uint32_t MATERIAL_FORMAT_VERSION = 4;
    // Version 3 adds conventional LOD and meshlet metadata. Version 2 remains
    // readable and is represented as a single legacy LOD.
    static constexpr uint32_t MESH_FORMAT_VERSION = 4;
    static constexpr uint32_t ANIMATION_CLIP_FORMAT_VERSION = 1;
    // Version 2 removes duplicate glyph storage and persists the tab width.
    // Version 3 persists complete layout metrics and the MSDF pixel range.
    // Version 4 persists fallback font asset references.
    static constexpr uint32_t FONT_FORMAT_VERSION = 4;
    static constexpr uint32_t AUDIO_FORMAT_VERSION = 1;
    static constexpr uint32_t AUDIO_MIXER_FORMAT_VERSION = 1;
    // Version 2 adds material combine modes and an asset header. Headerless
    // PhysicsMaterial2D payloads remain readable as version 1.
    static constexpr uint32_t PHYSICS_MATERIAL_FORMAT_VERSION = 2;
    static constexpr uint32_t NODEGRAPH_FORMAT_VERSION = 1;
    static constexpr uint32_t ENVIRONMENT_FORMAT_VERSION = 2;
    static constexpr uint32_t PREFAB_FORMAT_VERSION = 1;
    static constexpr uint32_t MATERIAL_PRESET_FORMAT_VERSION = 1;

    class Asset : public RefCounted
    {
    public:
        Asset() = default;
        virtual ~Asset() = default;
        virtual void Init() {};
        const String& GetName() const { return m_Name; }
        void SetName(const String& name) { m_Name = name; }

        virtual AssetType GetAssetType() const { return AssetType::None; }
        static AssetType GetStaticType() { return AssetType::None; }

        // Source tracking — used by editor to detect stale assets that need reimport.
        void SetSourceTimestamp(int64_t timestamp) { m_SourceTimestamp = timestamp; }
        int64_t GetSourceTimestamp() const { return m_SourceTimestamp; }
        void SetSourceContentHash(uint64_t hash) { m_SourceContentHash = hash; }
        uint64_t GetSourceContentHash() const { return m_SourceContentHash; }

    protected:
        CW_SERIALIZABLE(Asset);
        void AddDependency(const Ref<Asset>& asset);
        const Vector<Ref<Asset>>& GetDependencies() const;

        bool m_KeepData = true;
        int64_t m_SourceTimestamp = 0;
        uint64_t m_SourceContentHash = 0;
        String m_Name;
        Vector<Ref<Asset>> m_Dependencies;
    };

    struct AssetMetadata : public RefCounted
    {
        // TODO: Preview icons, with different sizes (256....16)
        UUID Uuid;                        // Asset UUID
        Ref<ImportOptions> ImportOptions; // Asset import options
        bool IncludeInBuild = false;
        AssetType Type = AssetType::None;
        String SubassetKey; // Stable importer identity for dependent assets. Empty only in version-one metadata.
    };

} // namespace Crowny
