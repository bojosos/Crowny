#pragma once

namespace Crowny
{
    template <class T>
    class Module
    {
    public:

        /**
         * @brief Returns a reference to the instance.
         * 
         * @return T reference
         */
        static T& Get()
        {
            if (!IsStartedUp())
                CW_ENGINE_ASSERT(false);
            if (IsDestroyed())
                CW_ENGINE_ASSERT(false);
            
            return *InstanceInternal();
        }
        
        /**
         * @brief Get a pointer to the instance.
         * 
         * @return T pointer
         */
        static T* GetPtr()
        {
            if (!IsStartedUp())
                CW_ENGINE_ASSERT(false);
            if (IsDestroyed())
                CW_ENGINE_ASSERT(false);
            
            return InstanceInternal();
        }
        
        /**
         * @brief Initializes the module.
         * 
         * @tparam Args 
         * @param args Arguments to forward to the constructor.
         */
        template<class... Args>
        static void StartUp(Args&&... args)
        {
            if (IsStartedUp())
                CW_ENGINE_ASSERT(false);
            InstanceInternal() = new T(std::forward<Args>(args)...);
            
            IsStartedUp() = true;
            ((Module*)InstanceInternal())->OnStartUp();
        }

        /**
         * @brief Initializes the module.
         * 
         * @tparam SubType Module subtype.
         * @tparam Args 
         * @param args Arguments to forward to the constructor.
         */
        template<class SubType, class... Args>
        static void StartUp(Args&&...args)
        {
            static_assert(std::is_base_of<T, SubType>::value, "Provided type is not derived from the initialization type.");
            
            if (IsStartedUp())
                CW_ENGINE_ASSERT(false);

            InstanceInternal() = new SubType(std::forward<Args>(args)...);
            IsStartedUp() = true;

            ((Module*)InstanceInternal())->OnStartUp();
        }
        
        /**
         * @brief Destroys the module.
         * 
         */
        static void Shutdown()
        {
            CW_ENGINE_INFO("Here");
            
            if (!IsStartedUp())
                CW_ENGINE_ASSERT(false);
            if (IsDestroyed())
                CW_ENGINE_ASSERT(false);
            
            ((Module*)InstanceInternal())->OnShutdown();
            delete InstanceInternal();
            IsDestroyed() = true;
        }
                
    protected:
        Module() = default;
        virtual ~Module() = default;
            
        Module(Module&&) = delete;
        Module(const Module&) = delete;
        Module& operator=(Module&&) = delete;
        Module& operator=(const Module&) = delete;
        
        virtual void OnStartUp() { }
        virtual void OnShutdown() { }
        
        static bool& IsDestroyed()
        {
            static bool s_Destroyed = false;
            return s_Destroyed;
        }
        
        static bool& IsStartedUp()
        {
            static bool s_StartedUp = false;
            return s_StartedUp;
        }
        
    private:
        static T*& InstanceInternal()
        {
            static T* s_Instance = nullptr;
            return s_Instance;
        }
    };
}
