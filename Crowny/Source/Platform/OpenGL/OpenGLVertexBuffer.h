#pragma once

#include "Crowny/Renderer/VertexBuffer.h"
#include "Crowny/Common/Types.h"

namespace Crowny
{

	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer(const void* data, uint32_t size, const VertexBufferProperties& props);
		~OpenGLVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual const BufferLayout& GetLayout() const override { return m_Layout; };
		virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
		virtual void SetData(const void* verts, uint32_t size) override;

		virtual void* GetPointer(uint32_t size) const override;
		virtual void FreePointer() const override;
	private:
		VertexBufferProperties m_Properties;
		uint32_t m_RendererID, m_Size;
		BufferLayout m_Layout;
	};
}