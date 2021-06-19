#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	enum class FramebufferTextureFormat
	{
		None = 0,

		RGB8 = 1,
		RGBA16F = 2,
		RGBA32F = 3,
		RG32F = 4,
		R32I = 5,
		DEPTH32F = 6,
		DEPTH24STENCIL8 = 7,
		RGBA8 = 8,
		Depth = DEPTH24STENCIL8
	};

	struct FramebufferTextureProperties
	{
		FramebufferTextureProperties() = default;
		FramebufferTextureProperties(FramebufferTextureFormat format) : TextureFormat(format) {}

		FramebufferTextureFormat TextureFormat;
	};

	struct FramebufferAttachmentProperties
	{
		FramebufferAttachmentProperties() = default;
		FramebufferAttachmentProperties(const std::initializer_list<FramebufferTextureProperties>& attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureProperties> Attachments;
	};
	
	struct FramebufferProperties
	{
		uint32_t Width = 0, Height = 0;
		glm::vec4 ClearColor;
		uint32_t Samples = 1;
		FramebufferAttachmentProperties Attachments;

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
		virtual Ref<Texture2D> GetColorAttachment(uint32_t slot = 0) const = 0;

		virtual const FramebufferProperties& GetProperties() const = 0;

		static Ref<Framebuffer> Create(const FramebufferProperties& props);
	};
}