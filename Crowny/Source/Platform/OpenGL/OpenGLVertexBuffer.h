#pragma once

#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Common/Types.h"

namespace Crowny
{

	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer(void* vertices, uint32_t size, BufferUsage usage);
		OpenGLVertexBuffer(uint32_t size, BufferUsage usage);
		~OpenGLVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; };
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

		virtual void* Map(uint32_t offset, uint32_t size, GpuLockOptions options) override;
		virtual void Unmap() override;
		
	private:
		uint32_t m_RendererID, m_Size;
		BufferLayout m_Layout;
	};
}