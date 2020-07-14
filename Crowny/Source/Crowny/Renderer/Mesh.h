#pragma once

namespace Crowny
{
	struct MeshData
	{
		uint32_t vertexCount;
		float* vertices;

		float* uvs;
		float* normals;
		uint32_t* indices;
	};

	class Mesh
	{
	public:
		virtual ~Mesh() = default;

		virtual MeshData& GetData() = 0;

		static Ref<Mesh> Create();
	private:
		MeshData m_MeshData;
	};
}