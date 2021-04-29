#include "cwpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Crowny
{

	void OpenGLMessageCallback(unsigned source, unsigned type, unsigned id, unsigned severity, int length, const char* message, const void* userParam)
	{
		CW_ENGINE_ASSERT(false);  // temporary
		switch (severity)
		{
			case GL_DEBUG_SEVERITY_HIGH:         CW_ENGINE_CRITICAL(message); return;
			case GL_DEBUG_SEVERITY_MEDIUM:       CW_ENGINE_ERROR(message);    return;
			case GL_DEBUG_SEVERITY_LOW:          CW_ENGINE_WARN(message);     return;
			case GL_DEBUG_SEVERITY_NOTIFICATION: CW_ENGINE_INFO(message);     return;
		}
	}

	void OpenGLRendererAPI::Init()
	{
		glEnable(GL_MULTISAMPLE); // TODO: Sample off-screen
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); 
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, 1280, 720);

#ifdef CW_DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif
	}
/*
	void OpenGLRendererAPI::SetDepthTest(bool value)
	{
		if (value)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	static uint32_t DrawModeToOpenGLMode(DrawMode mode)
	{
		switch (mode)
		{
			case DrawMode::POINT_LIST:         return GL_POINTS;
			case DrawMode::LINE_LIST:          return GL_LINES;
			case DrawMode::LINE_STRIP:         return GL_LINE_STRIP;
			case DrawMode::TRIANGLE_LIST:      return GL_TRIANGLES;
			case DrawMode::TRIANGLE_STRIP:     return GL_TRIANGLE_STRIP;
			case DrawMode::TRIANGLE_FAN:       return GL_TRIANGLE_FAN;
		}
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
	{
		uint32_t count = indexCount == -1 ? vertexArray->GetIndexBuffer()->GetCount() : indexCount;
		glDrawElements(DrawModeToOpenGLMode(vertexArray->GetDrawMode()), count, GL_UNSIGNED_INT, nullptr);
		glBindTexture(GL_TEXTURE_2D, 0);
	}*/
}
