#include "cwpch.h"

#include "Crowny/Renderer/RenderCommand.h"

namespace Crowny
{
	Scope<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}
