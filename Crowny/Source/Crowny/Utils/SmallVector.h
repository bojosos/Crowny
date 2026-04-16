#pragma once

#include "Crowny/Common/Assert.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Types.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>

namespace Crowny
{
    template <typename Type, uint32_t N> class SmallVector
    {
    public:
        using value_type = Type;
        using size_type = uint32_t;
        using difference_type = std::ptrdiff_t;
        using pointer = Type*;
        using const_pointer = const Type*;
        using reference = Type&;
        using const_reference = const Type&;

        using iterator = Type*;
        using const_iterator = const Type*;
        using reverse_iterator = std::reverse_iterator<Type*>;
        using const_reverse_iterator = std::reverse_iterator<const Type*>;

        using Iterator = Type*;
        using ConstIterator = const Type*;
        using ReverseIterator = std::reverse_iterator<Type*>;
        using ConstReverseIterator = std::reverse_iterator<const Type*>;

        SmallVector() : m_Elements(reinterpret_cast<Type*>(&m_StaticStorage)), m_Capacity(N), m_Size(0) {}

        SmallVector(uint32_t size, const Type& value) : SmallVector() { assign(size, value); }

        explicit SmallVector(uint32_t size) : SmallVector() { resize(size); }

        SmallVector(const SmallVector<Type, N>& other) : SmallVector() { assign(other.begin(), other.end()); }

        SmallVector(SmallVector&& other) noexcept : SmallVector()
        {
            if (!other.isStatic())
            {
                m_Elements = other.m_Elements;
                m_Capacity = other.m_Capacity;
                m_Size = other.m_Size;
                other.m_Elements = reinterpret_cast<Type*>(&other.m_StaticStorage);
                other.m_Capacity = N;
                other.m_Size = 0;
            }
            else
            {
                std::uninitialized_copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), m_Elements);
                m_Size = other.m_Size;
                other.clear();
            }
        }

        SmallVector(std::initializer_list<Type> list) : SmallVector() { assign(list.begin(), list.end()); }

        ~SmallVector()
        {
            destroyElements(begin(), end());
            if (!isStatic())
                ::operator delete(m_Elements);
        }

        SmallVector<Type, N>& operator=(const SmallVector<Type, N>& other)
        {
            if (this != &other)
                assign(other.begin(), other.end());
            return *this;
        }

        SmallVector<Type, N>& operator=(SmallVector<Type, N>&& other) noexcept
        {
            if (this == &other)
                return *this;

            clear();
            if (!isStatic())
                ::operator delete(m_Elements);

            if (!other.isStatic())
            {
                m_Elements = other.m_Elements;
                m_Capacity = other.m_Capacity;
                m_Size = other.m_Size;
                other.m_Elements = reinterpret_cast<Type*>(&other.m_StaticStorage);
                other.m_Capacity = N;
                other.m_Size = 0;
            }
            else
            {
                m_Elements = reinterpret_cast<Type*>(&m_StaticStorage);
                m_Capacity = N;
                std::uninitialized_copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), m_Elements);
                m_Size = other.m_Size;
                other.clear();
            }
            return *this;
        }

        bool operator==(const SmallVector<Type, N>& other) const
        {
            if (m_Size != other.m_Size)
                return false;
            return std::equal(begin(), end(), other.begin());
        }

        bool operator!=(const SmallVector<Type, N>& other) const { return !(*this == other); }

        Type& operator[](uint32_t index)
        {
            CW_ENGINE_ASSERT(index < m_Size);
            return m_Elements[index];
        }
        const Type& operator[](uint32_t index) const
        {
            CW_ENGINE_ASSERT(index < m_Size);
            return m_Elements[index];
        }

        Iterator begin() { return m_Elements; }
        Iterator end() { return m_Elements + m_Size; }
        ConstIterator begin() const { return m_Elements; }
        ConstIterator end() const { return m_Elements + m_Size; }
        ConstIterator cbegin() const { return begin(); }
        ConstIterator cend() const { return end(); }
        ReverseIterator rbegin() { return ReverseIterator(end()); }
        ReverseIterator rend() { return ReverseIterator(begin()); }
        ConstReverseIterator rbegin() const { return ConstReverseIterator(end()); }
        ConstReverseIterator rend() const { return ConstReverseIterator(begin()); }

        uint32_t size() const { return m_Size; }
        uint32_t capacity() const { return m_Capacity; }
        bool empty() const { return m_Size == 0; }
        Type* data() { return m_Elements; }
        const Type* data() const { return m_Elements; }

        Type& front()
        {
            CW_ENGINE_ASSERT(!empty());
            return m_Elements[0];
        }
        const Type& front() const
        {
            CW_ENGINE_ASSERT(!empty());
            return m_Elements[0];
        }
        Type& back()
        {
            CW_ENGINE_ASSERT(!empty());
            return m_Elements[m_Size - 1];
        }
        const Type& back() const
        {
            CW_ENGINE_ASSERT(!empty());
            return m_Elements[m_Size - 1];
        }

        void push_back(const Type& element)
        {
            if (m_Size == m_Capacity)
                grow(m_Capacity * 2);
            new (&m_Elements[m_Size++]) Type(element);
        }

        void push_back(Type&& element)
        {
            if (m_Size == m_Capacity)
                grow(m_Capacity * 2);
            new (&m_Elements[m_Size++]) Type(std::move(element));
        }

        template <typename... Args> void emplace_back(Args&&... args)
        {
            if (m_Size == m_Capacity)
                grow(m_Capacity * 2);
            new (&m_Elements[m_Size++]) Type(std::forward<Args>(args)...);
        }

        void pop_back()
        {
            CW_ENGINE_ASSERT(!empty());
            m_Size--;
            m_Elements[m_Size].~Type();
        }

        void clear()
        {
            destroyElements(begin(), end());
            m_Size = 0;
        }

        void resize(uint32_t size, const Type& value = Type())
        {
            if (size < m_Size)
            {
                destroyElements(begin() + size, end());
            }
            else if (size > m_Size)
            {
                if (size > m_Capacity)
                    grow(size);
                std::uninitialized_fill_n(end(), size - m_Size, value);
            }
            m_Size = size;
        }

        void reserve(uint32_t capacity)
        {
            if (capacity > m_Capacity)
                grow(capacity);
        }

        void assign(uint32_t size, const Type& value)
        {
            clear();
            if (size > m_Capacity)
                grow(size);
            std::uninitialized_fill_n(m_Elements, size, value);
            m_Size = size;
        }

        template <typename InputIt> void assign(InputIt first, InputIt last)
        {
            clear();
            uint32_t count = static_cast<uint32_t>(std::distance(first, last));
            if (count > m_Capacity)
                grow(count);
            std::uninitialized_copy(first, last, m_Elements);
            m_Size = count;
        }

        Iterator erase(ConstIterator iter)
        {
            CW_ENGINE_ASSERT(iter >= begin() && iter < end());
            Iterator it = const_cast<Iterator>(iter);
            destroyElements(it, it + 1);
            std::move(it + 1, end(), it);
            m_Size--;
            return it;
        }

    private:
        bool isStatic() const { return m_Elements == reinterpret_cast<const Type*>(&m_StaticStorage); }

        void grow(uint32_t newCapacity)
        {
            if (newCapacity <= m_Capacity)
                return;
            if (newCapacity < m_Capacity * 2)
                newCapacity = m_Capacity * 2;

            Type* newData = static_cast<Type*>(::operator new(newCapacity * sizeof(Type)));
            if (m_Size > 0)
            {
                std::uninitialized_copy(std::make_move_iterator(begin()), std::make_move_iterator(end()), newData);
                destroyElements(begin(), end());
            }

            if (!isStatic())
                ::operator delete(m_Elements);

            m_Elements = newData;
            m_Capacity = newCapacity;
        }

        void destroyElements(Iterator first, Iterator last)
        {
            for (; first != last; ++first)
                first->~Type();
        }

    private:
        std::aligned_storage_t<sizeof(Type), alignof(Type)> m_StaticStorage[N];
        Type* m_Elements;
        uint32_t m_Capacity;
        uint32_t m_Size;
    };
} // namespace Crowny
