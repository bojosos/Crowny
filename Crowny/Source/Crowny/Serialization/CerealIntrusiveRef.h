#pragma once

#include "Crowny/Common/RefCounted.h"

#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>

#include <memory>
#include <new>
#include <type_traits>
#include <typeindex>

class BinaryDataStreamInputArchive;

namespace cereal
{
    namespace intrusive_detail
    {
        template <typename T> std::shared_ptr<T> MakeTrackingPointer(Crowny::IntrusiveRef<T> pointer)
        {
            if (!pointer)
                return {};

            T* raw = pointer.Get();
            return std::shared_ptr<T>(raw, [owner = std::move(pointer)](T*) mutable { owner.Reset(); });
        }

        template <typename T> void* AllocateObjectStorage()
        {
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
                return ::operator new(sizeof(T), std::align_val_t(alignof(T)));
#endif
            return ::operator new(sizeof(T));
        }

        template <typename T> void FreeObjectStorage(void* storage) noexcept
        {
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
            if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(storage, std::align_val_t(alignof(T)));
                return;
            }
#endif
            ::operator delete(storage);
        }

        template <class Archive, class T>
        std::enable_if_t<!traits::has_load_and_construct<T, Archive>::value> LoadPointer(Archive& archive, Crowny::IntrusiveRef<T>& pointer)
        {
            uint32_t id = 0;
            archive(CEREAL_NVP_("id", id));

            if ((id & detail::msb_32bit) == 0)
            {
                const std::shared_ptr<void> tracked = archive.getSharedPointer(id);
                pointer = tracked ? Crowny::IntrusiveRef<T>(static_cast<T*>(tracked.get())) : nullptr;
                return;
            }

            using NonConstT = std::remove_const_t<T>;
            NonConstT* raw = detail::Construct<NonConstT, Archive>::load_andor_construct();
            Crowny::IntrusiveRef<NonConstT> owner(raw);
            archive.registerSharedPointer(id, MakeTrackingPointer(owner));
            pointer = Crowny::IntrusiveRef<T>(raw);
            archive(CEREAL_NVP_("data", *raw));
        }

        template <class Archive, class T>
        std::enable_if_t<traits::has_load_and_construct<T, Archive>::value> LoadPointer(Archive& archive, Crowny::IntrusiveRef<T>& pointer)
        {
            uint32_t id = 0;
            archive(CEREAL_NVP_("id", id));

            if ((id & detail::msb_32bit) == 0)
            {
                const std::shared_ptr<void> tracked = archive.getSharedPointer(id);
                pointer = tracked ? Crowny::IntrusiveRef<T>(static_cast<T*>(tracked.get())) : nullptr;
                return;
            }

            using NonConstT = std::remove_const_t<T>;
            NonConstT* raw = static_cast<NonConstT*>(AllocateObjectStorage<NonConstT>());
            auto owner = std::make_shared<Crowny::IntrusiveRef<NonConstT>>();
            auto constructed = std::make_shared<bool>(false);
            std::shared_ptr<void> tracked(raw, [owner, constructed](void* storage) mutable {
                if (*constructed)
                    owner->Reset();
                else
                    FreeObjectStorage<NonConstT>(storage);
            });
            archive.registerSharedPointer(id, tracked);

            memory_detail::LoadAndConstructLoadWrapper<Archive, NonConstT> loadWrapper(raw, [=]() mutable {
                *owner = Crowny::IntrusiveRef<NonConstT>(raw);
                *constructed = true;
            });
            archive(CEREAL_NVP_("data", loadWrapper));
            if (!*constructed)
                throw Exception("load_and_construct did not construct an IntrusiveRef target");
            pointer = Crowny::IntrusiveRef<T>(raw);
        }

        template <class Archive, class T> void SavePointer(Archive& archive, const Crowny::IntrusiveRef<T>& pointer)
        {
            const std::shared_ptr<T> tracked = MakeTrackingPointer(pointer);
            const uint32_t id = archive.registerSharedPointer(tracked);
            archive(CEREAL_NVP_("id", id));
            if (id & detail::msb_32bit)
                archive(CEREAL_NVP_("data", *pointer));
        }
    } // namespace intrusive_detail

    template <class Archive, class T>
    std::enable_if_t<!std::is_polymorphic_v<T>> CEREAL_SAVE_FUNCTION_NAME(Archive& archive, const Crowny::IntrusiveRef<T>& pointer)
    {
        intrusive_detail::SavePointer(archive, pointer);
    }

    template <class Archive, class T>
    std::enable_if_t<!std::is_polymorphic_v<T>> CEREAL_LOAD_FUNCTION_NAME(Archive& archive, Crowny::IntrusiveRef<T>& pointer)
    {
        intrusive_detail::LoadPointer(archive, pointer);
    }

    template <class Archive, class T>
    std::enable_if_t<std::is_polymorphic_v<T>> CEREAL_SAVE_FUNCTION_NAME(Archive& archive, const Crowny::IntrusiveRef<T>& pointer)
    {
        if (!pointer)
        {
            archive(CEREAL_NVP_("polymorphic_id", uint32_t(0)));
            return;
        }

        const std::type_info& dynamicType = typeid(*pointer);
        static const std::type_info& staticType = typeid(T);
        if constexpr (!std::is_abstract_v<T>)
        {
            if (dynamicType == staticType)
            {
                archive(CEREAL_NVP_("polymorphic_id", detail::msb2_32bit));
                intrusive_detail::SavePointer(archive, pointer);
                return;
            }
        }

        const auto& bindings = detail::StaticObject<detail::OutputBindingMap<Archive>>::getInstance().map;
        const auto binding = bindings.find(std::type_index(dynamicType));
        if (binding == bindings.end())
            throw Exception("Trying to save an unregistered IntrusiveRef polymorphic type (" + util::demangle(dynamicType.name()) + ")");
        binding->second.shared_ptr(&archive, pointer.Get(), staticType);
    }

    template <class Archive, class T>
    std::enable_if_t<std::is_polymorphic_v<T>> CEREAL_LOAD_FUNCTION_NAME(Archive& archive, Crowny::IntrusiveRef<T>& pointer)
    {
        uint32_t nameId = 0;
        archive(CEREAL_NVP_("polymorphic_id", nameId));

        if (nameId & detail::msb2_32bit)
        {
            if constexpr ((traits::is_default_constructible<T>::value || traits::has_load_and_construct<T, Archive>::value) && !std::is_abstract_v<T>)
            {
                intrusive_detail::LoadPointer(archive, pointer);
                return;
            }
            throw Exception("Cannot load an IntrusiveRef target that is abstract or not constructible");
        }

        auto binding = polymorphic_detail::getInputBinding(archive, nameId);
        std::shared_ptr<void> result;
        binding.shared_ptr(&archive, result, typeid(T));
        pointer = result ? Crowny::IntrusiveRef<T>(static_cast<T*>(result.get())) : nullptr;
    }

    namespace detail
    {
        template <class T> struct InputBindingCreator<BinaryDataStreamInputArchive, T>
        {
            InputBindingCreator()
            {
                auto& map = StaticObject<InputBindingMap<BinaryDataStreamInputArchive>>::getInstance().map;
                auto lock = StaticObject<InputBindingMap<BinaryDataStreamInputArchive>>::lock();
                auto key = std::string(binding_name<T>::name());
                const auto existing = map.lower_bound(key);
                if (existing != map.end() && existing->first == key)
                    return;

                typename InputBindingMap<BinaryDataStreamInputArchive>::Serializers serializers;
                serializers.shared_ptr = [](void* archivePointer, std::shared_ptr<void>& derivedPointer, const std::type_info& baseType) {
                    auto& archive = *static_cast<BinaryDataStreamInputArchive*>(archivePointer);
                    if constexpr (std::is_base_of_v<Crowny::RefCounted, T>)
                    {
                        Crowny::IntrusiveRef<T> pointer;
                        intrusive_detail::LoadPointer(archive, pointer);
                        derivedPointer = PolymorphicCasters::template upcast<T>(intrusive_detail::MakeTrackingPointer(std::move(pointer)), baseType);
                    }
                    else
                    {
                        std::shared_ptr<T> pointer;
                        archive(make_nvp<BinaryDataStreamInputArchive>("ptr_wrapper", memory_detail::make_ptr_wrapper(pointer)));
                        derivedPointer = PolymorphicCasters::template upcast<T>(pointer, baseType);
                    }
                };
                serializers.unique_ptr = [](void* archivePointer, std::unique_ptr<void, EmptyDeleter<void>>& derivedPointer,
                                            const std::type_info& baseType) {
                    auto& archive = *static_cast<BinaryDataStreamInputArchive*>(archivePointer);
                    std::unique_ptr<T> pointer;
                    archive(make_nvp<BinaryDataStreamInputArchive>("ptr_wrapper", memory_detail::make_ptr_wrapper(pointer)));
                    derivedPointer.reset(PolymorphicCasters::template upcast<T>(pointer.release(), baseType));
                };
                map.insert(existing, { std::move(key), std::move(serializers) });
            }
        };
    } // namespace detail
} // namespace cereal
