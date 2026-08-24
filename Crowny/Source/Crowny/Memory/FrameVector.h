#pragma once

#include "Crowny/Common/Assert.h"
#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    /**
     * Storage for data rebuilt once per frame.
     *
     * Reset only rewinds the active range. Constructed slots stay alive so nested
     * vectors and strings can reuse their capacity on the next frame. Acquire must
     * only be used when the caller overwrites every field it reads that frame.
     */
    template <typename T> class FrameVector
    {
    public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = const T*;

        T& Acquire()
        {
            if (m_ActiveSize == m_Storage.size())
                m_Storage.emplace_back();

            return m_Storage[m_ActiveSize++];
        }

        void Reset() noexcept { m_ActiveSize = 0; }

        void Reserve(size_t capacity) { m_Storage.reserve(capacity); }

        void Release()
        {
            m_Storage.clear();
            m_Storage.shrink_to_fit();
            m_ActiveSize = 0;
        }

        size_t Size() const noexcept { return m_ActiveSize; }
        size_t RetainedSize() const noexcept { return m_Storage.size(); }
        size_t Capacity() const noexcept { return m_Storage.capacity(); }
        bool Empty() const noexcept { return m_ActiveSize == 0; }

        T& operator[](size_t index)
        {
            CW_ENGINE_ASSERT(index < m_ActiveSize);
            return m_Storage[index];
        }

        const T& operator[](size_t index) const
        {
            CW_ENGINE_ASSERT(index < m_ActiveSize);
            return m_Storage[index];
        }

        iterator begin() noexcept { return m_Storage.data(); }
        iterator end() noexcept { return m_ActiveSize == 0 ? m_Storage.data() : m_Storage.data() + m_ActiveSize; }
        const_iterator begin() const noexcept { return m_Storage.data(); }
        const_iterator end() const noexcept { return m_ActiveSize == 0 ? m_Storage.data() : m_Storage.data() + m_ActiveSize; }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }

    private:
        Vector<T> m_Storage;
        size_t m_ActiveSize = 0;
    };
} // namespace Crowny
