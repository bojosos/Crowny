#include "cwpch.h"

#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/Camera.h"

#include <glm/ext/matrix_transform.hpp>
#include <glad/glad.h>
#include <thread>

namespace Crowny
{
/*
	struct RendererData
	{
		std::thread RenderThread;
		std::queue<std::function<void>(void)> CommandQueue;
		bool Running = false;
	};

	static RendererData s_Data;
*/
	void Renderer::Init()
	{/*
		s_Data.Running = true;
		s_Data.RenderThread = std::thread([]() {
			while (s_Data.Running)
			{
				if (!s_Data.CommandQueue.empty())
				{
					s_Data.CommandQueue.front()();
					s_Data.CommandQueue.pop();
				}
			}
		});
		
		s_Data.RenderThread.detach();
		SubmitCommand([](){ RenderCommand::Init(); });*/
		RenderCommand::Init();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}
/*
	void Renderer::SubmitCommand(std::function<void(void)> func)
	{
		s_Data.CommandQueue.push(func);
	}*/

	void Renderer::Shutdown()
	{
		//s_Data.Running = false;
	}

}