#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	enum class RenderSurfaceFormat
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

	struct RenderTextureSurface
	{
		RenderTextureSurface() = default;
		RenderTextureSurface(RenderSurfaceFormat format) : SurfaceFormat(format) {}

		RenderSurfaceFormat SurfaceFormat;
	};

	struct RenderTextureAttachments
	{
		RenderTextureAttachments() = default;
		RenderTextureAttachments(const std::initializer_list<RenderTextureSurface>& attachments)
			: Attachments(attachments) {}

		std::vector<RenderTextureSurface> Attachments;
	};
	
	struct RenderTextureProperties
	{
		uint32_t Width = 0, Height = 0;
		glm::vec4 ClearColor;
		uint32_t Samples = 1;
		RenderTextureAttachments Attachments;
	};
	
	class RenderTargetProperties
	{
	public:
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 0;
		bool SwapChainTarget = false;
	};
	
	class RenderTarget
	{
	public:
		virtual ~RenderTarget() = default;

		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual const RenderTargetProperties& GetProperties() const = 0;
		virtual void SwapBuffers(uint32_t syncMask = 0xFFFFFFFF) = 0;
	};
}