#pragma once

namespace Crowny
{
	struct FramebufferProperties
	{
		uint32_t Width = 0, Height = 0;
		uint32_t Samples = 1;

		bool SwapChainTarget = false;
	};

	class Framebuffer
	{
	public:

		virtual ~Framebuffer() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual uint32_t GetColorAttachmentRendererID() const = 0;

		virtual const FramebufferProperties& GetProperties() const = 0;

		static Ref<Framebuffer> Create(const FramebufferProperties& props);
	};
}