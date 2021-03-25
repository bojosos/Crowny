#pragma once

namespace Crowny
{
    template <class T>
    class Module
    {
    public:
        static T& Get()
        {
            if (!IsInitialized())
                CW_ENGINE_ASSERT(false);
            if (IsDestroyed())
                CW_ENGINE_ASSERT(false);
            
            return *InstanceInternal();
        }
        
        static T* GetPtr()
        {
            if (!IsInitialized())
                CW_ENGINE_ASSERT(false);
            if (IsDestroyed())
                CW_ENGINE_ASSERT(false);
            
            return InstanceInternal();
        }
        
        template<class... Args>
        static void Init(Args&&... args)
        {
            if (IsInitialized())
                CW_ENGINE_ASSERT(false);
            InstanceInternal() = new T(std::forward<Args>(args)...);
            
            IsInitialized() = true;
            ((Module*)InstanceInternal())->OnInitialize();
        }
        
        static void Shutdown()
        {
            if (!IsInitialized())
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
        
        virtual void OnInitialize() { }
        virtual void OnShutdown() { }
        
        static bool& IsDestroyed()
        {
            static bool s_Destroyed = false;
            return s_Destroyed;
        }
        
        static bool& IsInitialized()
        {
            static bool s_Initialized = false;
            return s_Initialized;
        }
        
    private:
        static T*& InstanceInternal()
        {
            static T* s_Instance = nullptr;
            return s_Instance;
        }
    };
}
