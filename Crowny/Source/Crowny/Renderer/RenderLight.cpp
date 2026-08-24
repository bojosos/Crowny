#include "cwpch.h"

#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;

        float LocalLightIntensity(const RenderLightDesc& desc, float outerCosine)
        {
            if (desc.Type == LightType::Directional)
                return std::max(desc.Intensity, 0.0f);
            if (desc.Type == LightType::Point)
                return std::max(desc.Intensity, 0.0f) / (4.0f * Pi);

            const float solidAngle = std::max(2.0f * Pi * (1.0f - outerCosine), 0.001f);
            return std::max(desc.Intensity, 0.0f) / solidAngle;
        }

        float LinearChannel(float srgb)
        {
            return srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        }
    } // namespace

    RenderLightWorld::RenderLightWorld(uint32_t initialCapacity) { Reserve(initialCapacity); }

    RenderLightHandle RenderLightWorld::CreateLight(const RenderLightDesc& desc)
    {
        ScopedLock lock(m_Mutex);
        uint32_t slotIndex = 0;
        if (!m_FreeSlots.empty())
        {
            slotIndex = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else
        {
            if (m_Slots.size() >= RenderLightHandle::MaxLights)
                return {};
            slotIndex = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }

        Slot& slot = m_Slots[slotIndex];
        const RenderLightHandle handle = RenderLightHandle::FromParts(slotIndex, slot.Generation);
        slot.Data = BuildLightData(desc, { handle.GetValue() });
        slot.Alive = true;
        m_ActiveLights++;
        QueueChange(slotIndex, handle, RenderLightChangeType::Create);
        return handle;
    }

    bool RenderLightWorld::UpdateLight(RenderLightHandle handle, const RenderLightDesc& desc)
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;
        Slot& slot = m_Slots[handle.GetIndex()];
        slot.Data = BuildLightData(desc, { handle.GetValue() });
        QueueChange(handle.GetIndex(), handle, RenderLightChangeType::Update);
        return true;
    }

    bool RenderLightWorld::DestroyLight(RenderLightHandle handle)
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;

        Slot& slot = m_Slots[handle.GetIndex()];
        slot.Alive = false;
        m_ActiveLights--;
        if (slot.PendingChange != InvalidChangeIndex &&
            m_PendingChanges[slot.PendingChange].Type == RenderLightChangeType::Create)
        {
            m_PendingChanges[slot.PendingChange].Type = RenderLightChangeType::Cancelled;
        }
        else
        {
            QueueChange(handle.GetIndex(), handle, RenderLightChangeType::Destroy);
        }
        slot.Generation = NextGeneration(slot.Generation);
        m_DeferredFreeSlots.push_back(handle.GetIndex());
        return true;
    }

    bool RenderLightWorld::IsAlive(RenderLightHandle handle) const
    {
        ScopedLock lock(m_Mutex);
        return ValidateHandle(handle);
    }

    bool RenderLightWorld::TryGetLight(RenderLightHandle handle, RenderLightData& output) const
    {
        ScopedLock lock(m_Mutex);
        if (!ValidateHandle(handle))
            return false;
        output = m_Slots[handle.GetIndex()].Data;
        return true;
    }

    uint32_t RenderLightWorld::GetActiveLightCount() const
    {
        ScopedLock lock(m_Mutex);
        return m_ActiveLights;
    }

    void RenderLightWorld::Reserve(uint32_t capacity)
    {
        ScopedLock lock(m_Mutex);
        const uint32_t clampedCapacity = std::min(capacity, RenderLightHandle::MaxLights);
        m_Slots.reserve(clampedCapacity);
        m_FreeSlots.reserve(clampedCapacity);
        m_DeferredFreeSlots.reserve(clampedCapacity);
        m_PendingChanges.reserve(clampedCapacity);
    }

    void RenderLightWorld::DrainChanges(Vector<RenderLightChange>& output)
    {
        ScopedLock lock(m_Mutex);
        output.clear();
        output.reserve(std::max(output.capacity(), m_PendingChanges.size()));
        for (const RenderLightChange& change : m_PendingChanges)
        {
            if (change.Type != RenderLightChangeType::Cancelled)
                output.push_back(change);
            const uint32_t slotIndex = change.Handle.GetIndex();
            if (slotIndex < m_Slots.size())
                m_Slots[slotIndex].PendingChange = InvalidChangeIndex;
        }
        m_PendingChanges.clear();

        for (uint32_t slotIndex : m_DeferredFreeSlots)
        {
            m_Slots[slotIndex].Data = {};
            m_FreeSlots.push_back(slotIndex);
        }
        m_DeferredFreeSlots.clear();
    }

    RenderLightData RenderLightWorld::BuildLightData(const RenderLightDesc& desc, RenderObjectID fallbackObjectID)
    {
        const float innerAngle = std::clamp(desc.SpotInnerAngle, 0.0f, Pi);
        const float outerAngle = std::clamp(desc.SpotOuterAngle, innerAngle, Pi);
        const float innerCosine = std::cos(innerAngle * 0.5f);
        const float outerCosine = std::cos(outerAngle * 0.5f);
        const glm::vec3 direction = glm::dot(desc.Direction, desc.Direction) > 0.000001f
                                      ? glm::normalize(desc.Direction)
                                      : glm::vec3(0.0f, 0.0f, -1.0f);

        RenderLightData data;
        data.PositionRange = { desc.Position, desc.Type == LightType::Directional ? 0.0f : std::max(desc.Range, 0.001f) };
        data.DirectionOuterCosine = { direction, desc.Type == LightType::Spot ? outerCosine : -1.0f };
        data.ColorIntensity = { glm::max(desc.Color, glm::vec3(0.0f)), LocalLightIntensity(desc, outerCosine) };
        data.SpotSourceAndBias = { innerCosine, std::max(desc.SourceRadius, 0.0f), std::max(desc.Shadows.Bias, 0.0f),
                                   std::max(desc.Shadows.NormalBias, 0.0f) };

        RenderLightFlags flags = desc.Flags;
        if (desc.Shadows.Mode != LightShadowMode::Disabled)
            flags = flags | RenderLightFlags::CastShadows;
        const RenderObjectID objectID = desc.ObjectID.IsValid() ? desc.ObjectID : fallbackObjectID;
        data.Metadata = { static_cast<uint32_t>(desc.Type), static_cast<uint32_t>(flags), desc.VisibilityLayers.Value,
                          objectID.Value };
        return data;
    }

    glm::vec3 RenderLightWorld::ColorTemperatureToLinearRgb(float kelvin)
    {
        const float temperature = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
        glm::vec3 srgb;
        srgb.r = temperature <= 66.0f ? 1.0f : std::clamp(1.292936186f * std::pow(temperature - 60.0f, -0.1332047592f), 0.0f, 1.0f);
        srgb.g = temperature <= 66.0f
                   ? std::clamp(0.390081579f * std::log(temperature) - 0.631841444f, 0.0f, 1.0f)
                   : std::clamp(1.129890861f * std::pow(temperature - 60.0f, -0.0755148492f), 0.0f, 1.0f);
        srgb.b = temperature >= 66.0f
                   ? 1.0f
                   : (temperature <= 19.0f
                        ? 0.0f
                        : std::clamp(0.543206789f * std::log(temperature - 10.0f) - 1.19625409f, 0.0f, 1.0f));
        return { LinearChannel(srgb.r), LinearChannel(srgb.g), LinearChannel(srgb.b) };
    }

    uint32_t RenderLightWorld::NextGeneration(uint32_t generation)
    {
        generation = (generation + 1u) & RenderLightHandle::GenerationMask;
        return generation == 0 ? 1u : generation;
    }

    bool RenderLightWorld::ValidateHandle(RenderLightHandle handle) const
    {
        if (!handle.IsValid() || handle.GetIndex() >= m_Slots.size())
            return false;
        const Slot& slot = m_Slots[handle.GetIndex()];
        return slot.Alive && slot.Generation == handle.GetGeneration();
    }

    void RenderLightWorld::QueueChange(uint32_t slotIndex, RenderLightHandle handle, RenderLightChangeType type)
    {
        Slot& slot = m_Slots[slotIndex];
        if (slot.PendingChange != InvalidChangeIndex)
        {
            RenderLightChange& pending = m_PendingChanges[slot.PendingChange];
            pending.Data = slot.Data;
            if (pending.Type != RenderLightChangeType::Create)
                pending.Type = type;
            return;
        }

        slot.PendingChange = static_cast<uint32_t>(m_PendingChanges.size());
        m_PendingChanges.push_back({ handle, type, slot.Data });
    }

} // namespace Crowny
