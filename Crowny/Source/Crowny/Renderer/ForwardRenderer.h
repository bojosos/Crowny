#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Model.h"

namespace Crowny
{
	class ForwardRenderer
	{
	public:
		static void Init();
		static void Begin();
		static void BeginScene(Camera* camera, const glm::mat4& transform);
		static void Submit(const Ref<Model>& model);
		static void SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform);
		//static void Submit(const RenderCommand& command);
		//static void SubmitLightSetup(const LightSetup& setup);
		static void SubmitLightSetup();
		static void EndScene();
		static void End();
		static void Flush();

	private:
		static void SetSystemUniforms(const Ref<Shader>& shader);
	};
}