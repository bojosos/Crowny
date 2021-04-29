#pragma once

#include "Crowny/Common/StringUtils.h"

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
        CAPS_CATEGORY_COUNT = 12
    };

    struct Conventions
    {
        enum class Axis : uint8_t
        {
            Up, Down
        };
        
        enum class MatrixOrder : uint8_t
        {
            ColumnMajor, RowMajor
        };

        Axis UvYAxis = Axis::Down;
        Axis YAxis = Axis::Up;
        Conventions::MatrixOrder MatrixOrder = Conventions::MatrixOrder::RowMajor;
    };

    struct DriverVersion
    {
        DriverVersion() = default;

        std::string ToString() const
        {
            std::stringstream str;
            str << major << "." << minor << "." << release << "." << build;
            return str.str();
        }

        void FromString(const std::string& version)
        {
            std::vector<std::string> toks = StringUtils::SplitString(version, ".");

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
        std::string RenderAPIName;
        Crowny::DriverVersion DriverVersion;
        std::string DeviceName;
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

        float horizontalTextelOffset = 0.0f;
        float verticalTexelOffset = 0.0f;

        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;

        Crowny::Conventions Conventions;

        void SetCapability(const Capabilities c)
        {
           // uint64_t idx = (CAPS_CATEGORY_MASK & c) >> BS_CAPS_BITSHIFT;
           // m_Capabilities[idx] |= (c & ~CAPS_CATEGORY_MASK);
        }

        bool HasCapability(const Capabilities c) const
        {
            //uint64_t idx = (CAPS_CATEGORY_MASK & c) >> BS_CAPS_BITSHIFT;
            //return (m_Capabilities[idx] & (c & ~CAPS_CATEGORY_MASK)) != 0;
            return true;
        }

        void AddShaderProfile(const std::string& profile)
        {
            m_SupportedShaderProfiles.insert(profile);
        }

        bool IsShaderProfileSupported(const std::string& profile) const
        {
            return(m_SupportedShaderProfiles.find(profile) != m_SupportedShaderProfiles.end());
        }

        const std::unordered_set<std::string>& GetSupportedShaderProfiles() const
            {
            return m_SupportedShaderProfiles;
            }

        static GPUVendor VendorFromString(const std::string& vendorString);
        static std::string VendorToString(GPUVendor vendor);

    private:
        static char const* const GPU_VENDOR_STRINGS[GPU_VENDOR_COUNT];

        uint32_t m_Capabilities[CAPS_CATEGORY_COUNT]{ 0 };
        std::unordered_set<std::string> m_SupportedShaderProfiles;
    };

}