#pragma once

#include "Crowny/Renderer/Framebuffer.h"

namespace Crowny
{
	class OpenGLFramebuffer : public Framebuffer 
	{
	public:
		OpenGLFramebuffer(const FramebufferProperties& props);
		~OpenGLFramebuffer();

		void Invalidate();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetColorAttachmentRendererID() const override { return m_ColorAttachment; }

		virtual const FramebufferProperties& GetProperties() const override { return m_Properties; }

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_ColorAttachment = 0, m_DepthAttachment = 0;
		FramebufferProperties m_Properties;

	};

}