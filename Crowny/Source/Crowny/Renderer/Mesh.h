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
    class Skeleton;

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

    struct Meshlet
    {
        uint32_t VertexOffset = 0;
        uint32_t TriangleOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;
        uint32_t MaterialSlot = 0;
        float LodError = 0.0f;
        glm::vec4 BoundingSphere = glm::vec4(0.0f);
        glm::vec4 NormalCone = glm::vec4(0.0f);
        CW_SIMPLESERIALIZABLE(Meshlet);
    };

    struct MeshLodSubMesh
    {
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;
        uint32_t MaterialSlot = 0;
        CW_SIMPLESERIALIZABLE(MeshLodSubMesh);
    };

    struct MeshLod
    {
        uint32_t FirstSubMesh = 0;
        uint32_t SubMeshCount = 0;
        uint32_t FirstMeshlet = 0;
        uint32_t MeshletCount = 0;
        float Error = 0.0f;
        CW_SIMPLESERIALIZABLE(MeshLod);
    };

    struct MeshGpuGeometry
    {
        Vector<uint32_t> LodIndices;
        Vector<MeshLodSubMesh> LodSubMeshes;
        Vector<MeshLod> Lods;
        Vector<Meshlet> Meshlets;
        Vector<uint32_t> MeshletVertices;
        Vector<uint8_t> MeshletTriangles;
        // Expanded global vertex indices for indexed-indirect meshlet draws.
        // Offsets match Meshlet::TriangleOffset and contain TriangleCount * 3 entries.
        Vector<uint32_t> MeshletIndices;

        bool IsEmpty() const { return Lods.empty(); }
    };

    class MeshData : public RefCounted
    {
    public:
        MeshData() = default;
        MeshData(uint32_t numVertices, uint32_t numIndices, const BufferLayout& layout, IndexType indexType = IndexType::Index_32);
        ~MeshData() override;
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
        uint8_t* m_Data = nullptr;
        uint32_t m_NumIndices = 0;
        uint32_t m_NumVertices = 0;
        IndexType m_IndexType = IndexType::Index_32;
        BufferLayout m_Layout;
    };

    struct MeshDesc
    {
        Ref<MeshData> Data; // null = allocate empty GPU buffers (Path B)
        MeshUsageFlags Usage = MeshUsage::Static;
        DrawMode Topology = DrawMode::TRIANGLE_LIST;
        Ref<MeshMorph> Morph = nullptr;
        Ref<Skeleton> MeshSkeleton = nullptr;
        Vector<SubMesh> SubMeshes;
        MeshGpuGeometry GpuGeometry;
        // Path B only — ignored when Data is set:
        uint32_t VertexCount = 0;
        uint32_t IndexCount = 0;
        BufferLayout Layout;
        IndexType IdxType = IndexType::Index_32;
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
        IndexType GetIndexType() const { return m_IndexType; }
        const BufferLayout& GetVertexLayout() const { return m_Layout; }
        const Vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
        const AABox& GetBounds() const { return m_AABox; }
        const SphereBounds& GetSphereBounds() const { return m_SphereBounds; }
        const MeshGpuGeometry& GetGpuGeometry() const { return m_GpuGeometry; }
        uint64_t GetGpuVersion() const { return m_GpuVersion; }

        // Mesh modification API
        void SetMeshData(const Ref<MeshData>& data);
        Ref<MeshData> GetMeshData() const;
        void UploadToGpu();
        void RecalculateBounds();
        void RecalculateNormals();
        void RecalculateTangents();
        bool IsDirty() const { return m_Dirty; }
        bool IsDynamic() const { return m_Usage.IsSet(MeshUsage::Dynamic); }
        bool IsCpuCached() const { return m_Usage.IsSet(MeshUsage::CpuCached); }

        Ref<MeshMorph> GetMorph() const { return m_MeshMorph; }
        Ref<Skeleton> GetSkeleton() const { return m_Skeleton; }

        static Ref<Mesh> Create(const MeshDesc& desc);

    protected:
        Mesh(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes, MeshUsageFlags usage, DrawMode drawMode, const Ref<MeshMorph>& morphs,
             const Ref<Skeleton>& skeleton);
        Mesh(const Vector<SubMesh>& subMeshes, uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout, MeshUsageFlags usage,
             DrawMode drawMode, IndexType indexType, const Ref<MeshMorph>& morphs = nullptr, const Ref<Skeleton>& skeleton = nullptr);

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
        MeshGpuGeometry m_GpuGeometry;
        uint32_t m_NumIndices;
        uint32_t m_NumVertices;
        IndexType m_IndexType;
        BufferLayout m_Layout;
        AABox m_AABox;
        SphereBounds m_SphereBounds;
        bool m_Dirty = false;
        uint64_t m_GpuVersion = 1;
        Ref<MeshMorph> m_MeshMorph;
        Ref<Skeleton> m_Skeleton;
    };
} // namespace Crowny
