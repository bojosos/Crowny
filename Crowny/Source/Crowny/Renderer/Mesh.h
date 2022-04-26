#pragma once

#include "Crowny/Assets/Asset.h"

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 Uv;
        glm::vec3 Tangent;
    };

    class Mesh : public Asset
    {

    public:
        Mesh(const Ref<VertexBuffer>& vbo, const Ref<IndexBuffer>& ibo);
        ~Mesh() = default;

        virtual AssetType GetAssetType() const override { return AssetType::Mesh; }
        static AssetType GetStaticType() { return AssetType::Mesh; }

        void Draw();
        Ref<MaterialInstance> GetMaterialInstance() { return m_MaterialInstance; }
        void SetMaterialInstance(const Ref<MaterialInstance>& materialInstance)
        {
            m_MaterialInstance = materialInstance;
        }

    private:
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<MaterialInstance> m_MaterialInstance;
    };
} // namespace Crowny
