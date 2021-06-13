#include "cwpch.h"

#include "Crowny/Renderer/UniformBufferBlock.h"
#include "Crowny/Renderer/Renderer.h"

//#include "Platform/OpenGL/OpenGLUniformBuffer.h"
#include "Platform/Vulkan/VulkanUniformBufferBlock.h"

namespace Crowny
{

    Ref<UniformBufferBlock> UniformBufferBlock::Create(uint32_t size, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
		{
//			case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, usage);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanUniformBufferBlock>(size, usage);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}
		
		return nullptr;
    }
	
	UniformBufferBlock::~UniformBufferBlock()
	{
		if (m_CachedData != nullptr)
			delete[] m_CachedData;
	}

	void UniformBufferBlock::UniformBufferBlock(uint32_t size, BufferUsage usage)
		: m_CachedData(nullptr), m_Usage(usage), m_Size(size)
	{
		if (m_Size > 0)
		{
			m_CachedData = new uint8_t[m_Size];
			memset(m_CachedData, 0, m_Size);
		}
	}

	void UniformBufferBlock::Write(uint32_t offset, const void* data, uint32_t size)
	{
		CW_ENGINE_ASSERT(offset + size > m_Size);
	}

	void UniformBufferBlock::FlushToGpu()
	{
		if (!m_BufferDirty)
		{
			void* dest = m_Buffer->Map(0, m_Size, WRITE_DISCARD);
			memcpy(dest, m_CachedData, m_Size);
			m_Buffer->Unmap();
			m_BufferDirty = false;
		}
	}

}