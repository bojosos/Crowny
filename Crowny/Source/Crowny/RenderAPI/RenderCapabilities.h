#pragma once

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{

    enum Capabilities : uint64_t
    {
        CW_TEXTURE_COMPRESSION_BC = 0,
        CW_TEXTURE_COMPRESSION_ETC2 = 1,
        CW_TEXTURE_COMPRESSION_ASTC = 2,

        CW_GEOMETRY_SHADER = 3,
        CW_TESSELLATION_SHADER = 4,
        CW_COMPUTE_SHADER = 5,
        CW_LOAD_STORE = 6,
        CW_LOAD_STORE_MSAA = 7,

        CW_TEXTURE_VIEWS = 8,
        CW_BYTECODE_CACHING = 9,
        CW_RENDER_TARGET_LAYERS = 10,
        CW_MULTITHREADED_CB = 11,
        CW_MULTI_DRAW_INDIRECT = 12,
        CW_DRAW_INDIRECT_COUNT = 13,
        CW_SHADER_DRAW_PARAMETERS = 14,
        CW_DESCRIPTOR_INDEXING = 15,
        CW_NON_UNIFORM_TEXTURE_INDEXING = 16,
        CW_UPDATE_AFTER_BIND = 17,
        CW_BUFFER_DEVICE_ADDRESS = 18,
        CW_TIMELINE_SEMAPHORE = 19,
        CW_SYNCHRONIZATION_2 = 20,
        CW_DYNAMIC_RENDERING = 21,
        CW_MESH_SHADER = 22,
        CW_RAY_TRACING = 23,
        CW_DEDICATED_COMPUTE_QUEUE = 24,
        CW_DEDICATED_TRANSFER_QUEUE = 25,
        CW_TEXTURE_COMPRESSION_BPTC = 26,
        CAPS_CATEGORY_COUNT = 27
    };

    struct Conventions
    {
        enum class Axis : uint8_t
        {
            Up,
            Down
        };

        enum class MatrixOrder : uint8_t
        {
            ColumnMajor,
            RowMajor
        };

        Axis UvYAxis = Axis::Down;
        Axis YAxis = Axis::Up;
        Conventions::MatrixOrder MatrixOrder = Conventions::MatrixOrder::RowMajor;
    };

    struct DriverVersion
    {
        DriverVersion() = default;

        String ToString() const
        {
            StringStream str;
            str << major << "." << minor << "." << release << "." << build;
            return str.str();
        }

        void FromString(const String& version)
        {
            Vector<String> toks = StringUtils::SplitString(version, ".");

            if (!toks.empty()) // TODO: Parse string
            {
                major = 1;
                if (toks.size() > 1)
                    minor = StringUtils::ParseInt(toks[0]);
                if (toks.size() > 2)
                    release = 3;
                if (toks.size() > 3)
                    build = 4;
            }
        }

        int32_t major = 0;
        int32_t minor = 0;
        int32_t release = 0;
        int32_t build = 0;
    };

    enum GPUVendor
    {
        GPU_UNKNOWN = 0,
        GPU_NVIDIA = 1,
        GPU_AMD = 2,
        GPU_INTEL = 3,
        GPU_VENDOR_COUNT = 4
    };

    // TODO: String id, with hashes
    class RenderCapabilities
    {
    public:
        String RenderAPIName;
        Crowny::DriverVersion DriverVersion;
        String DeviceName;
        GPUVendor DeviceVendor = GPU_UNKNOWN;
        uint16_t NumTextureUnitsPerStage[SHADER_COUNT] = { 0 };
        uint16_t NumCombinedTextureUnits = 0;
        uint16_t NumGpuParamBlockBuffersPerStage[SHADER_COUNT]{ 0 };
        uint16_t NumCombinedParamBlockBuffers = 0;
        uint16_t NumLoadStoreTextureUnitsPerStage[SHADER_COUNT]{ 0 };
        uint16_t NumCombinedLoadStoreTextureUnits = 0;
        uint16_t MaxBoundVertexBuffers = 0;
        uint16_t NumMultiRenderTargets = 0;
        uint16_t GeometryShaderNumOutputVertices = 0;
        uint32_t MaxDrawIndirectCount = 0;
        uint32_t MaxBindlessSampledImages = 0;
        uint64_t MaxStorageBufferRange = 0;
        bool IntegratedGpu = false;

        float horizontalTextelOffset = 0.0f;
        float verticalTexelOffset = 0.0f;

        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;

        Crowny::Conventions Conventions;

        void SetCapability(const Capabilities c)
        {
            const uint64_t index = static_cast<uint64_t>(c);
            if (index < CAPS_CATEGORY_COUNT)
                m_Capabilities[index] = true;
        }

        void UnsetCapability(const Capabilities c)
        {
            const uint64_t index = static_cast<uint64_t>(c);
            if (index < CAPS_CATEGORY_COUNT)
                m_Capabilities[index] = false;
        }

        bool HasCapability(const Capabilities c) const
        {
            const uint64_t index = static_cast<uint64_t>(c);
            return index < CAPS_CATEGORY_COUNT && m_Capabilities[index];
        }

        RenderFeatureTier GetFeatureTier() const;
        RenderingPath ResolveRenderingPath(RenderingPath requested) const;

        void AddShaderProfile(const String& profile) { m_SupportedShaderProfiles.insert(profile); }

        bool IsShaderProfileSupported(const String& profile) const
        {
            return (m_SupportedShaderProfiles.find(profile) != m_SupportedShaderProfiles.end());
        }

        const Set<String>& GetSupportedShaderProfiles() const { return m_SupportedShaderProfiles; }

        static GPUVendor VendorFromString(const String& vendorString);
        static String VendorToString(GPUVendor vendor);

    private:
        static char const* const GPU_VENDOR_STRINGS[GPU_VENDOR_COUNT];

        Array<bool, CAPS_CATEGORY_COUNT> m_Capabilities{};
        Set<String> m_SupportedShaderProfiles;
    };

} // namespace Crowny
