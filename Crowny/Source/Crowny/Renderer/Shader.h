#pragma once

namespace Crowny
{

	class Shader
	{
	public:
		virtual void Reload() = 0;

		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetIntV(const std::string& name, uint32_t count, int* ptr) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		
		virtual void UploadUniformInt(const std::string& name, int value) = 0;
		virtual void UploadUniformIntV(const std::string& name, int count, int* ptr) = 0;
		virtual void UploadUniformFloat(const std::string& name, float value) = 0;
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& value) = 0;
		
		virtual void UploadUniformMat3(const std::string& name, const glm::mat3& matrix) = 0;
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;

		virtual uint32_t GetUniformLocation(const std::string& name) = 0;
		virtual void RetrieveLocations(const std::vector<std::string>& uniforms) = 0;

		virtual const std::string& GetName() const = 0;

		static Ref<Shader> Create(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
		static Ref<Shader> Create(const std::string& filepath);
	};

	class ShaderLibrary
	{
	public:
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

	private:
		std::string m_Name;
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}