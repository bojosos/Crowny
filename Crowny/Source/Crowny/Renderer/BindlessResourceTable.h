#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{
    class BindlessResourceHandle
    {
    public:
        static constexpr uint32_t IndexBits = 20;
        static constexpr uint32_t IndexMask = (1u << IndexBits) - 1u;
        static constexpr uint32_t GenerationMask = (1u << (32u - IndexBits)) - 1u;
        static constexpr uint32_t MaxResources = 1u << IndexBits;

        constexpr BindlessResourceHandle() = default;
        static constexpr BindlessResourceHandle FromParts(uint32_t index, uint32_t generation)
        {
            return index <= IndexMask && generation != 0 && generation <= GenerationMask
                     ? BindlessResourceHandle((generation << IndexBits) | index)
                     : BindlessResourceHandle();
        }

        constexpr bool IsValid() const { return m_Value != 0; }
        constexpr uint32_t GetIndex() const { return m_Value & IndexMask; }
        constexpr uint32_t GetGeneration() const { return m_Value >> IndexBits; }
        constexpr uint32_t GetValue() const { return m_Value; }
        explicit constexpr operator bool() const { return IsValid(); }
        bool operator==(const BindlessResourceHandle& other) const = default;

    private:
        explicit constexpr BindlessResourceHandle(uint32_t value) : m_Value(value) {}
        uint32_t m_Value = 0;
    };

    struct BindlessResourceUpdate
    {
        uint32_t DescriptorIndex = 0;
        uint64_t ResourceID = 0;
    };

    // Backend-neutral lifetime table used by Vulkan descriptor indexing and by
    // descriptor-page fallbacks. Slot zero is permanently reserved for the
    // missing-resource descriptor.
    class BindlessResourceTable
    {
    public:
        explicit BindlessResourceTable(uint32_t capacity, uint64_t fallbackResourceID = 0);

        BindlessResourceHandle Allocate(uint64_t resourceID);
        bool Replace(BindlessResourceHandle handle, uint64_t resourceID);
        bool Release(BindlessResourceHandle handle, uint64_t retireValue);
        void Collect(uint64_t completedValue);

        uint64_t Resolve(BindlessResourceHandle handle) const;
        uint32_t GetDescriptorIndex(BindlessResourceHandle handle) const;
        uint32_t GetCapacity() const { return static_cast<uint32_t>(m_Slots.size()); }
        uint32_t GetActiveCount() const;
        BindlessResourceHandle GetFallbackHandle() const { return BindlessResourceHandle::FromParts(0, 1); }
        void DrainUpdates(Vector<BindlessResourceUpdate>& output);

    private:
        struct Slot
        {
            uint64_t ResourceID = 0;
            uint32_t Generation = 1;
            bool Occupied = false;
            bool Dirty = false;
        };

        struct RetiredSlot
        {
            uint32_t Index = 0;
            uint64_t RetireValue = 0;
        };

        static uint32_t NextGeneration(uint32_t generation);
        bool Validate(BindlessResourceHandle handle) const;

        mutable Mutex m_Mutex;
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_FreeSlots;
        Vector<RetiredSlot> m_RetiredSlots;
        uint64_t m_FallbackResourceID = 0;
        uint32_t m_ActiveCount = 0;
    };

} // namespace Crowny
