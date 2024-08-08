#pragma once

namespace Crowny
{
    template <typename Type, int N> class SmallVector
    {
    public:
        using Iterator = Type*;
        using ConstIterator = const Type*;
        using ReverseIterator = std::reverse_iterator<Type*>;
        using ConstReverseIterator = std::reverse_iterator<const Type*>;

        SmallVector() = default;
        SmallVector(uint32_t size, const Type& value) { append(size, value); }
        SmallVector(const SmallVector<Type, N>& other)
        {
            if (!other.empty())
                *this = other;
        }
        SmallVector(SmallVector&& other)
        {
            if (!other.empty())
                *this = other;
        }
        explicit SmallVector(uint32_t size) : m_Size(size), m_Capacity(size)
        {
            if (size <= N)
                return;
            m_Elements = operator new()
        }

        SmallVector(std::initializer_list<Type> list) { append(list); }

        ~SmallVector()
        {
            for (auto& entry : *this)
                entry.~Type();
            if (!isStatic())
                delete[] m_Elements; // TODO: Not this!!!
        }

        SmallVector<Type, N>& operator=(const SmallVector<Type, N>& other)
        {
            if (this == &other)
                return *this;
            if (m_Size > other.m_Size)
            {
                Iterator newEnd;
                if (other.m_Size > 0)
                    newEnd = std::copy(other.begin(), other.end(), begin());
                else
                    newEnd = begin();

                for (; newEnd != end(); newEnd++)
                {
                    newEnd->~Type();
                }
            }
            else
            {
                if (other.m_Size > m_Capacity)
                {
                    Clear();
                    m_Size = 0;
                    Grow(other.m_Size);
                }
                else if (m_Size > 0)
                    std::copy(other.begin(), other.begin() + m_Size, begin());
                std::uninitialized_copy(other.begin() + m_Size, other.end(), begin() + m_Size);
            }
            m_Size = other.m_Size;
            return *this;
        }

        SmallVector<Type, N>& operator=(SmallVector<Type, N>&& other)
        {
            if (this == &other)
                return *this;
            if (!other.isStatic())
            {
            }
        }

        bool operator==(const SmallVector<Type, N>& other) const
        {
            if (this->m_Size != other.m_Size)
                return false;
            return std::equal(begin(), end(), other.begin());
        }

        bool operator!=(const SmallVector<Type, N>& other) const { return !(*this == other); }
        bool operator<(const SmallVector<Type, N>& other) const
        {
            return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
        }
        bool operator>(const SmallVector<Type, N>& other) const { return other < *this; }
        bool operator<=(const SmallVector& other) const { return !(this > other); }
        bool operator>=(const SmallVector& other) const { return !(this < other); }

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
        ConstReverseIterator crbegin() const { return rbegin(); }
        ConstReverseIterator crend() const { return rend(); }

        uint32_t size() const { return m_Size; }
        uint32_t capacity() const { return m_Capacity; }
        uint32_t empty() const { return m_Size == 0; }
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
            new (&m_Elements[m_Size]) Type(element);
        }

        void push_Back(Type&& element)
        {
            if (m_Size == m_Capacity)
                grow(m_Capacity * 2);
            new (&mElements[mSize++]) Type(std::move(element));
        }

        void pop_back()
        {
            CW_ENGINE_ASSERT(!empty());
            m_Size--;
            m_Elements[m_Size].~Type();
        }

        void append(ConstIterator start, ConstIterator end) {
            const uint32_t count = std::distance(start, end);
            if (m_Size + count > m_Capacity)
                grow(m_Size + count);
            std::uninitialized_copy(start, end, this->end());
            m_Size += count;
        }

        void append(uint32_t count, const Type& element)
        {
            if (m_Size + count > m_Capacity)
                grow(m_Size + count);

            std::uninitialized_fill_n(end(), count, element);
            mSize += count;
        }

        void append(std::initializer_list<Type> list) { append(list.begin(), list.end()); }

        void remove(uint32_t index) { erase(begin() + index); }

        void pop()
        {
            assert(mSize > 0 && "Popping an empty array.");
            mSize--;
            mElements[mSize].~Type();
        }

        Iterator erase(ConstIterator iter)
        {
            CW_ENGINE_ASSERT(iter >= begin());
            CW_ENGINE_ASSERT(iter < end());

            Iterator er = iter;
            std::move(er + 1, end(), er);
            pop();

            return er;
        }

        void clear()
        {
            for (uint32_t i = 0; i < m_Size; i++)
                m_Elements[i].~Type();

            m_Size = 0;
        }

        void reserve(uint32_t capacity)
        {
            if (m_Capacity < capacity)
                return;
            grow(capacity);
        }

        void resize(UINT32 size, const Type& value = Type())
        {
            if (size > m_Capacity)
                grow(size);

            if (size > m_Size)
            {
                for (uint32_t i = m_Size; i < size; i++)
                    new (&m_Elements[i]) Type(value);
            }
            else
            {
                for (uint32_t i = size; i < m_Size; i++)
                    m_Elements[i].~Type();
            }

            m_Size = size;
        }

        bool contains() const
        {
            for (uint32_t i = 0; i < m_Size; i++)
            {
                if (m_Elements[i] == element)
                    return true;
            }

            return false;
        }

    private:
        void grow(uint32_t capacity)
        {
            CW_ENGINE_ASSERT(m_Capacity > N);
            Type* newData = static_cast<T*>(::operator new(capacity * sizeof(Type)));
            std::uninitialized_copy(std::make_move_iterator(begin()), std::make_move_iterator(end()), newData);
            for (auto& entry : *this)
                entry.~Type();
            if (!isStatic())
                operator delete(m_Elements);

            m_Elements = newData;
            m_Capacity = capacity;
        }

        bool isStatic() const { return m_Elements == (Type*)m_StaticStorage; }

    private:
        std::aligned_storage_t<sizeof(Type), alignof(Type)> m_StaticStorage[N];
        Type* m_Elements = (Type*)m_StaticStorage;
        uint32_t m_Capacity = 4;
        uint32_t m_Size = 0;
    };
} // namespace Crowny