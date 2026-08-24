#pragma once

#include "Crowny/Common/Assert.h"

#include <memory>
#include <type_traits>
#include <utility>

namespace Crowny
{
    template <class T> class Module
    {
    public:
        /**
         * @brief Returns a reference to the instance.
         *
         * @return T reference
         */
        static T& Get()
        {
            CW_ENGINE_ASSERT(IsStartedUp());
            CW_ENGINE_ASSERT(!IsDestroyed());

            return *InstanceInternal();
        }

        /**
         * @brief Get a pointer to the instance.
         *
         * @return T pointer
         */
        static T* GetPtr()
        {
            CW_ENGINE_ASSERT(IsStartedUp());
            CW_ENGINE_ASSERT(!IsDestroyed());

            return InstanceInternal();
        }

        /**
         * @brief Returns the instance when the module is running, or nullptr otherwise.
         */
        static T* TryGet() { return IsStartedUp() ? InstanceInternal() : nullptr; }

        /**
         * @brief Initializes the module.
         *
         * @tparam Args
         * @param args Arguments to forward to the constructor.
         */
        template <class... Args> static void StartUp(Args&&... args) { StartUp(std::make_unique<T>(std::forward<Args>(args)...)); }

        /**
         * @brief Initializes the module.
         *
         * @tparam SubType Module subtype.
         * @tparam Args
         * @param args Arguments to forward to the constructor.
         */
        template <class SubType, class... Args> static void StartUp(Args&&... args)
        {
            static_assert(std::is_base_of<T, SubType>::value, "Provided type is not derived from the initialization type.");

            std::unique_ptr<T> instance = std::make_unique<SubType>(std::forward<Args>(args)...);
            StartUp(std::move(instance));
        }

        static void StartUp(std::unique_ptr<T> instance)
        {
            CW_ENGINE_ASSERT(!IsStartedUp());
            CW_ENGINE_ASSERT(instance != nullptr);

            InstanceInternal() = instance.get();
            IsStartedUp() = true;
            IsDestroyed() = false;

            try
            {
                static_cast<Module*>(InstanceInternal())->OnStartUp();
            }
            catch (...)
            {
                InstanceInternal() = nullptr;
                IsStartedUp() = false;
                IsDestroyed() = true;
                throw;
            }

            instance.release();
        }

        /**
         * @brief Destroys the module.
         *
         */
        static void Shutdown()
        {
            if (!IsStartedUp())
                return;
            if (IsDestroyed())
                return;

            T* instance = InstanceInternal();
            static_cast<Module*>(instance)->OnShutdown();
            IsDestroyed() = true;
            IsStartedUp() = false;
            InstanceInternal() = nullptr;
            delete instance;
        }

        static bool& IsStartedUp()
        {
            static bool s_StartedUp = false;
            return s_StartedUp;
        }

    protected:
        Module() = default;
        virtual ~Module() = default;

        Module(Module&&) = delete;
        Module(const Module&) = delete;
        Module& operator=(Module&&) = delete;
        Module& operator=(const Module&) = delete;

        virtual void OnStartUp() {}
        virtual void OnShutdown() {}

        static bool& IsDestroyed()
        {
            static bool s_Destroyed = false;
            return s_Destroyed;
        }

    private:
        static T*& InstanceInternal()
        {
            // Module lifetime is explicit. A function-static smart pointer would
            // destroy a still-running module during static teardown without
            // calling OnShutdown, after its dependencies may already be gone.
            static T* s_Instance = nullptr;
            return s_Instance;
        }
    };
} // namespace Crowny
