#pragma once

#include "Crowny/Renderer/Decal.h"

namespace Crowny
{
    struct DecalHandle
    {
        uint32_t Index = 0xffffffffu;
        uint32_t Generation = 0;
        bool operator==(const DecalHandle&) const = default;
    };

    struct DecalRecord
    {
        UUID Id;
        UUID Material;
        DecalSettings Settings;
        glm::mat4 World{ 1.0f };
    };

    enum class DecalChangeType : uint8_t
    {
        Create,
        Update,
        Destroy
    };
    struct DecalChange
    {
        DecalHandle Handle;
        DecalChangeType Type;
        DecalRecord Record;
    };

    // Scene-owned change journal. Callers never hold pointers into reusable slots.
    class DecalWorld
    {
    public:
        DecalHandle Create(const DecalRecord& record);
        bool Update(DecalHandle handle, const DecalRecord& record);
        bool Destroy(DecalHandle handle);
        bool IsAlive(DecalHandle handle) const;
        bool TryGet(DecalHandle handle, DecalRecord& record) const;
        void DrainChanges(Vector<DecalChange>& changes);
        void Clear();

    private:
        struct Slot
        {
            DecalRecord Record;
            uint32_t Generation = 1;
            bool Alive = false;
        };
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_Free;
        Vector<DecalChange> m_Changes;
    };
} // namespace Crowny
