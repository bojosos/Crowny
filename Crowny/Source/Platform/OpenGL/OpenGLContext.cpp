#include "cwpch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Crowny
{
	OpenGLContext::OpenGLContext(void* window) : m_Window(window)
	{

	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent((GLFWwindow*)m_Window);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

		CW_ENGINE_ASSERT(status, "Failed to initialize GLAD!");

		CW_ENGINE_INFO("OpenGL Info:");
		CW_ENGINE_INFO("  Vendor: {0}", glGetString(GL_VENDOR));
		CW_ENGINE_INFO("  Renderer: {0}", glGetString(GL_RENDERER));
		CW_ENGINE_INFO("  Version: {0}", glGetString(GL_VERSION));

		int maxTextureSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

		int maxTextureSlots;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureSlots);

		int maxCombinedTextures;
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextures);

		CW_ENGINE_INFO("    Max Slots: {0}", maxTextureSlots);
		CW_ENGINE_INFO("    Max Total Textures: {0}", maxCombinedTextures);
		CW_ENGINE_INFO("    Max Size: {0}x{0}", maxTextureSize);
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers((GLFWwindow*)m_Window);
	}
}