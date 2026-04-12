#include "cwpch.h"

#include "Crowny/RenderAPI/AccelerationStructure.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Platform/Vulkan/VulkanAccelerationStructure.h"

namespace Crowny
{

    AccelerationStructure::AccelerationStructure(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel,
                                                 uint32_t maxTopLevelInstances, AccelerationStructBuildFlags flags)
    {
        m_AllowUpdate = flags.IsSet(AccelerationStructBuildBits::AllowUpdate);
        m_IsTopLevel = isTopLevel;
    }

    Ref<AccelerationStructure> AccelerationStructure::Create(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel,
                                                             uint32_t maxTopLevelInstances, AccelerationStructBuildFlags flags)
    {
        switch (gRenderAPI->GetAPI())
        {
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanAccelerationStructure>(topLevelInstances, isTopLevel, maxTopLevelInstances, flags);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
        return nullptr;
    }
} // namespace Crowny