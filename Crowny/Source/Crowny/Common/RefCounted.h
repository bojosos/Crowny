#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace Crowny
{

    /**
     * @brief Base class for all intrusive reference-counted objects.
     *
     * Inherit from this class for any type that will be owned via Ref<T>.
     * The ref count is stored inside the object (one allocation instead of two).
     * Thread-safe: AddRef/Release use atomic operations.
     */
    class RefCounted
    {
    public:
        void AddRef() const noexcept { m_RefCount.fetch_add(1, std::memory_order_relaxed); }

        void Release() const
        {
            if (m_RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                delete this;
        }

        uint32_t GetRefCount() const noexcept { return m_RefCount.load(std::memory_order_relaxed); }

        // Copy/move: don't propagate the ref count — the new object starts at 0.
        RefCounted(const RefCounted&) noexcept {}
        RefCounted& operator=(const RefCounted&) noexcept { return *this; }
        RefCounted(RefCounted&&) noexcept {}
        RefCounted& operator=(RefCounted&&) noexcept { return *this; }

    protected:
        RefCounted() noexcept = default;
        virtual ~RefCounted() = default;

    private:
        mutable std::atomic<uint32_t> m_RefCount = 0;
    };

    // -----------------------------------------------------------------------
    // IntrusiveRef<T> — intrusive shared ownership smart pointer
    // -----------------------------------------------------------------------

    template<typename T>
    class IntrusiveRef
    {
    public:
        IntrusiveRef() noexcept = default;
        IntrusiveRef(std::nullptr_t) noexcept {}

        // Requires T to be complete at point of construction (for the T*→RefCounted* cast),
        // but NOT at point of destruction — enabling use with forward-declared types as members.
        explicit IntrusiveRef(T* ptr) noexcept : m_Ptr(ptr), m_RefBase(static_cast<const RefCounted*>(ptr))
        {
            if (m_RefBase)
                m_RefBase->AddRef();
        }

        IntrusiveRef(const IntrusiveRef& o) noexcept : m_Ptr(o.m_Ptr), m_RefBase(o.m_RefBase)
        {
            if (m_RefBase)
                m_RefBase->AddRef();
        }

        IntrusiveRef(IntrusiveRef&& o) noexcept : m_Ptr(o.m_Ptr), m_RefBase(o.m_RefBase)
        {
            o.m_Ptr = nullptr;
            o.m_RefBase = nullptr;
        }

        // Implicit converting constructor — mirrors std::shared_ptr<T>(shared_ptr<U>):
        // allows Ref<Derived> -> Ref<Base> (upcast) AND Ref<T> -> Ref<const T> (cv-qualification).
        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        IntrusiveRef(const IntrusiveRef<U>& o) noexcept : m_Ptr(o.m_Ptr), m_RefBase(o.m_RefBase)
        {
            if (m_RefBase)
                m_RefBase->AddRef();
        }

        template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        IntrusiveRef(IntrusiveRef<U>&& o) noexcept : m_Ptr(o.m_Ptr), m_RefBase(o.m_RefBase)
        {
            o.m_Ptr = nullptr;
            o.m_RefBase = nullptr;
        }

        // m_RefBase is RefCounted* — no need for T to be complete here.
        ~IntrusiveRef() { if (m_RefBase) m_RefBase->Release(); }

        IntrusiveRef& operator=(const IntrusiveRef& o) noexcept
        {
            IntrusiveRef tmp(o);
            Swap(tmp);
            return *this;
        }

        IntrusiveRef& operator=(IntrusiveRef&& o) noexcept
        {
            IntrusiveRef tmp(std::move(o));
            Swap(tmp);
            return *this;
        }

        IntrusiveRef& operator=(std::nullptr_t) noexcept
        {
            Reset();
            return *this;
        }

        T*       Get()        const noexcept { return m_Ptr; }
        T*       get()        const noexcept { return m_Ptr; }  // shared_ptr compat alias
        T*       operator->() const noexcept { return m_Ptr; }
        T&       operator*()  const noexcept { return *m_Ptr; }
        explicit operator bool() const noexcept { return m_Ptr != nullptr; }

        bool operator==(const IntrusiveRef& o) const noexcept { return m_Ptr == o.m_Ptr; }
        bool operator==(std::nullptr_t)        const noexcept { return m_Ptr == nullptr; }
        bool operator< (const IntrusiveRef& o) const noexcept { return m_Ptr <  o.m_Ptr; }

        void Reset() noexcept
        {
            if (m_RefBase)
            {
                m_RefBase->Release();
                m_Ptr = nullptr;
                m_RefBase = nullptr;
            }
        }

        void Swap(IntrusiveRef& o) noexcept
        {
            std::swap(m_Ptr, o.m_Ptr);
            std::swap(m_RefBase, o.m_RefBase);
        }

        template<typename U>
        IntrusiveRef<U> StaticCast() const noexcept
        {
            return IntrusiveRef<U>(static_cast<U*>(m_Ptr));
        }

        template<typename U>
        IntrusiveRef<U> DynamicCast() const noexcept
        {
            return IntrusiveRef<U>(dynamic_cast<U*>(m_Ptr));
        }

    private:
        T*                m_Ptr     = nullptr;
        const RefCounted* m_RefBase = nullptr;  // Base pointer for AddRef/Release — no complete-type requirement at destruction

        // Allow IntrusiveRef<U> to access our members for converting constructors
        template<typename U> friend class IntrusiveRef;
    };

    // -----------------------------------------------------------------------
    // Free-function helpers (drop-in replacements for std:: casts)
    // -----------------------------------------------------------------------

    template<typename T, typename U>
    IntrusiveRef<T> StaticRefCast(const IntrusiveRef<U>& ref) noexcept
    {
        return ref.template StaticCast<T>();
    }

    template<typename T, typename U>
    IntrusiveRef<T> DynamicRefCast(const IntrusiveRef<U>& ref) noexcept
    {
        return ref.template DynamicCast<T>();
    }

    // -----------------------------------------------------------------------
    // CreateRef<T> — constructs T on the heap, wraps in IntrusiveRef
    // No separate control-block allocation (vs. std::make_shared).
    // -----------------------------------------------------------------------

    template<typename T, typename... Args>
    IntrusiveRef<T> CreateRef(Args&&... args)
    {
        return IntrusiveRef<T>(new T(std::forward<Args>(args)...));
    }

} // namespace Crowny

// std::hash specialisation so IntrusiveRef<T> can be used in unordered containers
namespace std
{
    template<typename T>
    struct hash<Crowny::IntrusiveRef<T>>
    {
        size_t operator()(const Crowny::IntrusiveRef<T>& ref) const noexcept
        {
            return hash<T*>()(ref.Get());
        }
    };
} // namespace std
