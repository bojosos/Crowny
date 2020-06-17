#pragma once

#include <glm/glm.hpp>
#include "Crowny/Renderer/Shader.h"

namespace Crowny
{
	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
		OpenGLShader(const std::string& filepath);
		~OpenGLShader();

		virtual void Reload() override;
		
		virtual void Bind() const override;
		virtual void Unbind() const override;
		
		virtual void SetInt(const std::string& name, int value) override;
		virtual void SetIntV(const std::string& name, uint32_t count, int* ptr) override;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) override;
		
		virtual void UploadUniformInt(const std::string& name, int value) override;
		virtual void UploadUniformIntV(const std::string& name, int count, int* ptr) override;
		virtual void UploadUniformFloat(const std::string& name, float value) override;
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& value) override;
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& value) override;
		
		virtual void UploadUniformMat3(const std::string& name, const glm::mat3& matrix) override;
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;
		
		virtual uint32_t GetUniformLocation(const std::string& name) override;
		virtual void RetrieveLocations(const std::vector<std::string>& uniforms) override;
		
		virtual const std::string& GetName() const override { return m_Name; };

	private:
		void Load(const std::string& path);
		void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);

	private:
		std::string m_Filepath;
		std::string m_Name;
		uint32_t m_RendererID;
		std::unordered_map<std::string, int32_t> m_UniformLocations;
	};

}
