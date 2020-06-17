#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Renderer/IndexBuffer.h"


namespace Crowny
{
	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer(uint32_t* indices, uint32_t size);
		OpenGLIndexBuffer(uint32_t size);
		virtual ~OpenGLIndexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual uint32_t GetCount() const { return m_Count; }
	private:
		uint32_t m_RendererID, m_Count;

	};
}
