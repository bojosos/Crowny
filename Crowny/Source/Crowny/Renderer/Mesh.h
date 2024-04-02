#pragma once

#include "Crowny/Assets/Asset.h"

#include "Crowny/Common/Types.h"

#include "Crowny/Math/AABox.h"
#include "Crowny/Math/SphereBounds.h"

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    enum class MeshUsage
    {
        Static = 0,
        Dynamic = 1,
        CpuCached = 2
    };
    typedef Flags<MeshUsage> MeshUsageFlags;
    CW_FLAGS_OPERATORS(MeshUsage);

    struct SubMesh
    {
        SubMesh(uint32_t indexOffset, uint32_t indexCount, DrawMode drawMode)
          : IndexOffset(IndexOffset), IndexCount(indexCount), MeshDrawMode(drawMode)
        {
        }
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;
        DrawMode MeshDrawMode = DrawMode::TRIANGLE_LIST;
    };

    class MeshData
    {
    public:
        MeshData() = default;
        MeshData(uint32_t numVertices, uint32_t numIndices, const BufferLayout& layout,
                 IndexType indexType = IndexType::Index_32);
        void SetVertexData(VertexAttribute attribute, const void* data, uint32_t size, uint32_t semanticIdx = 0,
                           uint32_t streamIdx = 0);
        void GetVertexData(VertexAttribute attribute, void* data, uint32_t size, uint32_t semanticIdx = 0,
                           uint32_t streamIdx = 0);
        template <typename IndexType> void SetIndexData(void* data, uint32_t indexCount)
        {
            CW_ENGINE_ASSERT(sizeof(IndexType) == GetIndexSize());
            std::memcpy(GetIndexData<IndexType>(), data, indexCount * GetIndexSize());
        }
        void SetIndexData(void* data, uint32_t indexCount);
        void AllocateBuffer();

        template <typename Type = uint8_t> Type* GetIndexData() const { return (Type*)m_Data; }
        uint32_t GetVertexCount() const { return m_NumVertices; }
        uint32_t GetIndexCount() const { return m_NumIndices; }
        const BufferLayout& GetBufferLayout() const { return m_Layout; }
        IndexType GetIndexType() const { return m_IndexType; }
        uint32_t GetIndexSize() const
        {
            return m_IndexType == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t);
        }
        uint32_t GetIndexBufferSize() const { return m_NumIndices * GetIndexSize(); }
        uint32_t GetVertexBufferSize() const { return m_Layout.GetStride() * m_NumVertices; }
        uint8_t* GetVerexBufferData() const { return m_Data + GetIndexBufferSize(); }

        uint8_t* GetElementData(const BufferElement& bufferElement) const;
        void CalculateBounds(AABox& outAABox, SphereBounds& outSphereBounds) const;
        static Ref<MeshData> Combine(const Vector<Ref<MeshData>>& meshes, const Vector<Vector<SubMesh>>& subMeshes,
                                     Vector<SubMesh>& outSubMeshes);

        static Ref<MeshData> Create(uint32_t vertexCount, uint32_t indexCount, const BufferLayout& bufferLayout,
                                    IndexType indexType = IndexType::Index_32);
        CW_SERIALIZABLE(MeshData);

    private:
        uint8_t* m_Data;
        uint32_t m_NumIndices;
        uint32_t m_NumVertices;
        IndexType m_IndexType;
        BufferLayout m_Layout;
    };

    class Mesh : public Asset
    {
    public:
        virtual ~Mesh() override;
        virtual void Init() override;

        virtual AssetType GetAssetType() const override { return AssetType::Mesh; }
        static AssetType GetStaticType() { return AssetType::Mesh; }

        Ref<MeshData> AllocBuffer() const;
        uint32_t GetIndexCount() const { return m_NumIndices; }
        uint32_t GetVertexCount() const { return m_NumVertices; }
        Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
        Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

        void UpdateCpuBuffer(const MeshData& meshData);

        void WriteData(const Ref<MeshData>& meshData, bool discard, bool updateBounds = true, int32_t queue = 0);
        void ReadData(Ref<MeshData>& data, uint32_t queueIdx = 0);
        DrawMode GetDrawMode() { return m_DrawMode; }

        static Ref<Mesh> Create(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes = {},
                                MeshUsageFlags usage = MeshUsage::Static, DrawMode drawMode = DrawMode::TRIANGLE_LIST);
        static Ref<Mesh> Create(uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout,
                                MeshUsageFlags usage, DrawMode drawMode, IndexType indexType);

        // protected:
        Mesh(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes, MeshUsageFlags usage, DrawMode drawMode);
        Mesh(const Vector<SubMesh>& subMeshes, uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout,
             MeshUsageFlags usage, DrawMode drawMode, IndexType indexType);

        Mesh() = default; // For serialization
    private:
        CW_SERIALIZABLE(Mesh);
        mutable Ref<MeshData> m_CPUMeshData;
        Ref<MeshData> m_InitialData;
        MeshUsageFlags m_Usage;
        DrawMode m_DrawMode;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Vector<SubMesh> m_SubMeshes;
        uint32_t m_NumIndices;
        uint32_t m_NumVertices;
        IndexType m_IndexType;
        BufferLayout m_Layout;
        AABox m_AABox;
        SphereBounds m_SphereBounds;
    };
} // namespace Crowny
