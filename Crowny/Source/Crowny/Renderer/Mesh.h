#pragma once

#include "Crowny/Assets/Asset.h"

#include "Crowny/Common/Types.h"

#include "Crowny/Animation/MorphAnimation.h"

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
          : IndexOffset(indexOffset), IndexCount(indexCount), MeshDrawMode(drawMode)
        {
        }
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;
        DrawMode MeshDrawMode = DrawMode::TRIANGLE_LIST;
    };

    class MeshData : public RefCounted
    {
    public:
        MeshData() = default;
        MeshData(uint32_t numVertices, uint32_t numIndices, const BufferLayout& layout, IndexType indexType = IndexType::Index_32);
        void SetVertexData(VertexAttribute attribute, const void* data, uint32_t size, uint32_t semanticIdx = 0, uint32_t streamIdx = 0);
        void GetVertexData(VertexAttribute attribute, void* data, uint32_t size, uint32_t semanticIdx = 0, uint32_t streamIdx = 0);
        template <typename IndexType> void SetIndexData(void* data, uint32_t indexCount)
        {
            CW_ENGINE_ASSERT(sizeof(IndexType) == GetIndexSize());
            std::memcpy(GetIndexData<IndexType>(), data, indexCount * GetIndexSize());
        }
        void SetIndexData(void* data, uint32_t indexCount);
        void AllocateBuffer();

        // Typed attribute accessors
        Vector<glm::vec3> GetPositions() const;
        void SetPositions(const Vector<glm::vec3>& positions);
        Vector<glm::vec3> GetNormals() const;
        void SetNormals(const Vector<glm::vec3>& normals);
        Vector<glm::vec3> GetTangents() const;
        void SetTangents(const Vector<glm::vec3>& tangents);
        Vector<glm::vec3> GetBitangents() const;
        void SetBitangents(const Vector<glm::vec3>& bitangents);
        Vector<glm::vec2> GetUVs(uint32_t channel = 0) const;
        void SetUVs(uint32_t channel, const Vector<glm::vec2>& uvs);
        Vector<glm::vec4> GetColors() const;
        void SetColors(const Vector<glm::vec4>& colors);
        Vector<uint32_t> GetIndices() const;
        void SetIndices(const Vector<uint32_t>& indices);

        template <typename Type = uint8_t> Type* GetIndexData() const { return (Type*)m_Data; }
        uint32_t GetVertexCount() const { return m_NumVertices; }
        uint32_t GetIndexCount() const { return m_NumIndices; }
        const BufferLayout& GetBufferLayout() const { return m_Layout; }
        IndexType GetIndexType() const { return m_IndexType; }
        uint32_t GetIndexSize() const { return m_IndexType == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t); }
        uint32_t GetIndexBufferSize() const { return m_NumIndices * GetIndexSize(); }
        uint32_t GetVertexBufferSize() const { return m_Layout.GetStride() * m_NumVertices; }
        uint8_t* GetVertexBufferData() const { return m_Data + GetIndexBufferSize(); }

        uint8_t* GetElementData(const BufferElement& bufferElement) const;
        void CalculateBounds(AABox& outAABox, SphereBounds& outSphereBounds) const;
        static Ref<MeshData> Combine(const Vector<Ref<MeshData>>& meshes, const Vector<Vector<SubMesh>>& subMeshes, Vector<SubMesh>& outSubMeshes);

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

    struct MeshDesc
    {
        Ref<MeshData>   Data;                                // null = allocate empty GPU buffers (Path B)
        MeshUsageFlags  Usage    = MeshUsage::Static;
        DrawMode        Topology = DrawMode::TRIANGLE_LIST;
        Ref<MeshMorph>  Morph    = nullptr;
        Vector<SubMesh> SubMeshes;
        // Path B only — ignored when Data is set:
        uint32_t        VertexCount = 0;
        uint32_t        IndexCount  = 0;
        BufferLayout    Layout;
        IndexType       IdxType     = IndexType::Index_32;
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
        DrawMode GetDrawMode() const { return m_DrawMode; }
        const Vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }

        // Mesh modification API
        void SetMeshData(const Ref<MeshData>& data);
        Ref<MeshData> GetMeshData() const;
        void UploadToGpu();
        void RecalculateBounds();
        void RecalculateNormals();
        void RecalculateTangents();
        bool IsDirty() const { return m_Dirty; }

        Ref<MeshMorph> GetMorph() const { return m_MeshMorph; }

        static Ref<Mesh> Create(const MeshDesc& desc);

    protected:
        Mesh(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes, MeshUsageFlags usage, DrawMode drawMode, const Ref<MeshMorph>& morphs);
        Mesh(const Vector<SubMesh>& subMeshes, uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout, MeshUsageFlags usage,
             DrawMode drawMode, IndexType indexType, const Ref<MeshMorph>& morphs = nullptr);

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
        bool m_Dirty = false;
        Ref<MeshMorph> m_MeshMorph;
    };
} // namespace Crowny
