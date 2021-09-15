#include "cwpch.h"

#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/Renderer/Renderer.h"

//#include "Platform/OpenGL/OpenGLUniformBuffer.h"
#include "Platform/Vulkan/VulkanUniformBufferBlock.h"

namespace Crowny
{

    Ref<UniformBufferBlock> UniformBufferBlock::Create(uint32_t size, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
        {
            //			case RenderAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, usage);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanUniformBufferBlock>(size, usage);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    UniformBufferBlock::~UniformBufferBlock()
    {
        if (m_CachedData != nullptr)
            delete[] m_CachedData;
    }

    UniformBufferBlock::UniformBufferBlock(uint32_t size, BufferUsage usage)
      : m_CachedData(nullptr), m_Usage(usage), m_Size(size), m_BufferDirty(false)
    {
        if (m_Size > 0)
        {
            m_CachedData = new uint8_t[m_Size];
            Cw_ZeroOut(m_CachedData, m_Size);
        }
    }

    void UniformBufferBlock::Write(uint32_t offset, const void* data, uint32_t size)
    {
        CW_ENGINE_ASSERT(offset + size <= m_Size);
        std::memcpy(m_CachedData + offset, data, size);
        m_BufferDirty = true;
    }

    void UniformBufferBlock::Read(uint32_t offset, void* data, uint32_t size)
    {
        CW_ENGINE_ASSERT(offset + size <= m_Size);
        std::memcpy(data, m_CachedData + offset, size);
    }

    void UniformBufferBlock::ZeroOut(uint32_t offset, uint32_t size)
    {
        CW_ENGINE_ASSERT(offset + size <= m_Size);
        std::memset(m_CachedData, offset, size);
        m_BufferDirty = true;
    }

    void UniformBufferBlock::FlushToGpu()
    {
        if (m_BufferDirty)
        {
            void* dest = m_Buffer->Map(0, m_Size, GpuLockOptions::WRITE_DISCARD);
            std::memcpy(dest, m_CachedData, m_Size);
            m_Buffer->Unmap();
            m_BufferDirty = false;
        }
    }

} // namespace Crowny