#pragma once

#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	
	class RenderTargetProperties
	{
	public:
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Samples = 0;
		bool SwapChainTarget = false;
		uint32_t NumSlices = 0;
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