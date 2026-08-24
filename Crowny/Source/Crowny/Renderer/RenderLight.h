#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{
    enum class LightType : uint8_t
    {
        Directional,
        Point,
        Spot
    };

    enum class LightShadowMode : uint8_t
    {
        Disabled,
        Hard,
        Soft
    };

    enum class RenderLightFlags : uint32_t
    {
        None = 0,
        Enabled = 1 << 0,
        CastShadows = 1 << 1,
        AffectDiffuse = 1 << 2,
        AffectSpecular = 1 << 3,
        Volumetric = 1 << 4,
        StaticShadowCaster = 1 << 5
    };

    constexpr RenderLightFlags operator|(RenderLightFlags first, RenderLightFlags second)
    {
        return static_cast<RenderLightFlags>(static_cast<uint32_t>(first) | static_cast<uint32_t>(second));
    }

    constexpr bool HasFlag(RenderLightFlags value, RenderLightFlags flag)
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    struct LightShadowSettings
    {
        LightShadowMode Mode = LightShadowMode::Disabled;
        float Bias = 0.001f;
        float NormalBias = 0.02f;
        float NearPlane = 0.05f;
        float Importance = 1.0f;
        uint16_t Resolution = 1024;
        bool CacheStaticCasters = true;

        bool operator==(const LightShadowSettings& other) const = default;
    };

    struct RenderLightDesc
    {
        LightType Type = LightType::Point;
        glm::vec3 Position = glm::vec3(0.0f);
        glm::vec3 Direction = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 Color = glm::vec3(1.0f);
        // Lux for directional lights and lumens for point and spot lights.
        float Intensity = 1000.0f;
        float Range = 10.0f;
        float SpotInnerAngle = glm::radians(25.0f);
        float SpotOuterAngle = glm::radians(35.0f);
        float SourceRadius = 0.0f;
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        RenderObjectID ObjectID;
        RenderLightFlags Flags = RenderLightFlags::Enabled | RenderLightFlags::AffectDiffuse |
                                 RenderLightFlags::AffectSpecular;
        LightShadowSettings Shadows;
    };

    // GPU-facing structure shared by Forward+, Deferred+, shadow scheduling,
    // and CPU reference tests. Intensity stores lux for directional lights and
    // candela for local lights.
    struct alignas(16) RenderLightData
    {
        glm::vec4 PositionRange = glm::vec4(0.0f);
        glm::vec4 DirectionOuterCosine = glm::vec4(0.0f, 0.0f, -1.0f, -1.0f);
        glm::vec4 ColorIntensity = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        glm::vec4 SpotSourceAndBias = glm::vec4(1.0f, 0.0f, 0.001f, 0.02f);
        glm::uvec4 Metadata = glm::uvec4(0u);
    };

    static_assert(sizeof(RenderLightData) == 80, "GPU light records must preserve std430 float4 alignment");

    enum class RenderLightChangeType : uint8_t
    {
        Create,
        Update,
        Destroy,
        Cancelled
    };

    struct RenderLightChange
    {
        RenderLightHandle Handle;
        RenderLightChangeType Type = RenderLightChangeType::Update;
        RenderLightData Data;
    };

    class RenderLightWorld
    {
    public:
        explicit RenderLightWorld(uint32_t initialCapacity = 256);

        RenderLightHandle CreateLight(const RenderLightDesc& desc);
        bool UpdateLight(RenderLightHandle handle, const RenderLightDesc& desc);
        bool DestroyLight(RenderLightHandle handle);

        bool IsAlive(RenderLightHandle handle) const;
        bool TryGetLight(RenderLightHandle handle, RenderLightData& output) const;
        uint32_t GetActiveLightCount() const;
        void Reserve(uint32_t capacity);
        void DrainChanges(Vector<RenderLightChange>& output);

        static RenderLightData BuildLightData(const RenderLightDesc& desc, RenderObjectID fallbackObjectID = {});
        static glm::vec3 ColorTemperatureToLinearRgb(float kelvin);

    private:
        static constexpr uint32_t InvalidChangeIndex = 0xffffffffu;

        struct Slot
        {
            RenderLightData Data;
            uint32_t Generation = 1;
            uint32_t PendingChange = InvalidChangeIndex;
            bool Alive = false;
        };

        static uint32_t NextGeneration(uint32_t generation);
        bool ValidateHandle(RenderLightHandle handle) const;
        void QueueChange(uint32_t slotIndex, RenderLightHandle handle, RenderLightChangeType type);

        mutable Mutex m_Mutex;
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_FreeSlots;
        Vector<uint32_t> m_DeferredFreeSlots;
        Vector<RenderLightChange> m_PendingChanges;
        uint32_t m_ActiveLights = 0;
    };

} // namespace Crowny
