#pragma once

#include "Crowny/Renderer/GraphicsContext.h"

namespace Crowny
{

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(void* window);

		virtual void Init() override;
		virtual void SwapBuffers() override;

	private:
		void* m_Window;
	};

}