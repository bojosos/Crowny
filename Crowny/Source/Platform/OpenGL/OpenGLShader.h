#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glm/glm.hpp>

namespace Crowny
{

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const BinaryShaderData& data) {}
		~OpenGLShader();
		
		virtual const Ref<UniformDesc>& GetUniformDesc() const override { return m_UniformDesc; }

	private:
		void Load(const std::string& path);
		std::unordered_map<uint32_t, std::string> ShaderPreProcess(const std::string& source);
		void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);

	private:
		Ref<UniformDesc> m_UniformDesc;
		std::string m_Filepath;
		std::string m_Name;
		uint32_t m_RendererID;
	};

}
