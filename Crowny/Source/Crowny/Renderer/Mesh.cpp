#include "cwpch.h"

#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/Renderer.h"

#include <glm/gtx/norm.hpp>

namespace Crowny
{

    MeshData::MeshData(uint32_t numVertices, uint32_t numIndices, const BufferLayout& layout, IndexType indexType)
      : m_NumVertices(numVertices), m_NumIndices(numIndices), m_Layout(layout), m_IndexType(indexType), m_Data(nullptr)
    {
        AllocateBuffer();
    }

    void MeshData::AllocateBuffer()
    {
        const uint32_t bufferSize = GetIndexBufferSize() + GetVertexBufferSize();
        CW_ENGINE_INFO(bufferSize);
        m_Data = new uint8_t[bufferSize];
    }

    void MeshData::SetVertexData(VertexAttribute attribute, const void* data, uint32_t size, uint32_t semanticIdx,
                                 uint32_t streamIdx)
    {
        const uint32_t offset = m_Layout.GetOffset(attribute);
        const uint32_t elementSize = m_Layout.GetElementSize(attribute);
        const uint32_t stride = m_Layout.GetStride();
        uint32_t indexBufferOffset = GetIndexBufferSize();

        uint8_t* dst = m_Data + indexBufferOffset + offset;
        uint8_t* src = (uint8_t*)data;
        if (size != elementSize * m_NumVertices)
        {
            CW_ENGINE_ERROR("Mismatched vertex data sizes");
            return;
        }

        for (uint32_t i = 0; i < m_NumVertices; i++)
        {
            CW_ENGINE_ASSERT(dst < m_Data + indexBufferOffset + GetVertexBufferSize());
            std::memcpy(dst, src, elementSize);
            dst += stride;
            src += elementSize;
        }
    }

    uint8_t* MeshData::GetElementData(const BufferElement& bufferElement) const
    {
        return m_Data + GetIndexBufferSize() + bufferElement.Offset;
    }

    void MeshData::CalculateBounds(AABox& outAABox, SphereBounds& outSphereBounds) const
    {
        for (const auto& element : m_Layout.GetElements())
        {
            if (element.Attribute != VertexAttribute::Position || element.Type != ShaderDataType::Float3 ||
                element.Type != ShaderDataType::Float4)
                continue;
            uint8_t* data = GetElementData(element);
            uint32_t stride = m_Layout.GetStride();
            if (m_NumVertices > 0)
            {
                glm::vec3 pos = *(glm::vec3*)data;
                glm::vec3 acc = pos;
                glm::vec3 minBounds = pos;
                glm::vec3 maxBounds = pos;
                float radiusSqrd = 0.0f;

                for (uint32_t i = 0; i < m_NumVertices; i++)
                {
                    pos = *((glm::vec3*)(data + stride * i));
                    acc += pos;
                    minBounds = glm::min(minBounds, pos);
                    maxBounds = glm::max(maxBounds, pos);
                }
                const glm::vec3 center = acc / (float)m_NumVertices;
                for (uint32_t i = 0; i < m_NumVertices; i++)
                {
                    const float dist = glm::distance2(pos, center);
                    radiusSqrd = std::max(radiusSqrd, dist);
                }
                outAABox = AABox(minBounds, maxBounds);
                outSphereBounds = SphereBounds(center, glm::sqrt(radiusSqrd));
                break;
            }
        }
    }

    Ref<MeshData> MeshData::Create(uint32_t vertexCount, uint32_t indexCount, const BufferLayout& bufferLayout,
                                   IndexType indexType)
    {
        return CreateRef<MeshData>(vertexCount, indexCount, bufferLayout, indexType);
    }

    Mesh::Mesh(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes, MeshUsageFlags usage, DrawMode drawMode)
      : m_SubMeshes(subMeshes), m_IndexType(meshData->GetIndexType()), m_NumVertices(meshData->GetVertexCount()),
        m_Layout(meshData->GetBufferLayout()), m_NumIndices(meshData->GetIndexCount()), m_CPUMeshData(meshData),
        m_DrawMode(drawMode), m_Usage(usage), m_InitialData(meshData)
    {
        Init();
    }

    Mesh::Mesh(const Vector<SubMesh>& subMeshes, uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout,
               MeshUsageFlags usage, DrawMode drawMode, IndexType indexType)
      : m_SubMeshes(subMeshes), m_NumVertices(vertexCount), m_NumIndices(indexCount), m_Layout(layout), m_Usage(usage),
        m_DrawMode(drawMode), m_IndexType(indexType), m_InitialData(nullptr)
    {
        Init();
    }

    void Mesh::WriteData(const Ref<MeshData>& meshData, bool discard, bool updateBounds, int32_t queue)
    {
        UpdateCpuBuffer(*meshData);
        if (discard)
        {
            if (m_Usage.IsSet(MeshUsage::Dynamic))
            {
                CW_ENGINE_WARN("Buffer discard enabled for non dynamic buffer, disabling it.");
                discard = false;
            }
        }
        else
        {
            if (m_Usage.IsSet(MeshUsage::Dynamic))
            {
                CW_ENGINE_WARN("Buffer dicard not enabled for dynamic mesh");
                discard = true;
            }
        }

        if (meshData->GetIndexCount() > m_IndexBuffer->GetCount())
        {
            CW_ENGINE_WARN("Provided index buffer is out of range: {0} > {1}", meshData->GetIndexCount(),
                           m_IndexBuffer->GetCount());
            return;
        }

        uint32_t indexBufferSize = meshData->GetIndexBufferSize();
        if (indexBufferSize != m_IndexBuffer->GetBufferSize())
        {
            indexBufferSize = m_IndexBuffer->GetBufferSize();
            CW_ENGINE_WARN("Provided index buffer has more data");
        }
        m_IndexBuffer->WriteData(0, indexBufferSize, meshData->GetIndexData(),
                                 discard ? BufferWriteOptions::BWT_DISCARD
                                         : BufferWriteOptions::BWT_NORMAL /*,  queue */);
        m_VertexBuffer->WriteData(0, meshData->GetVertexBufferSize(), meshData->GetVerexBufferData(),
                                  discard ? BufferWriteOptions::BWT_DISCARD : BufferWriteOptions::BWT_NORMAL);
        if (updateBounds)
            meshData->CalculateBounds(m_AABox, m_SphereBounds);
    }

    void Mesh::ReadData(Ref<MeshData>& data, uint32_t queueIdx)
    {
        RenderAPI::Get().SubmitCommandBuffer(nullptr);
        IndexType indexType = IndexType::Index_32;
        if (m_IndexBuffer)
            indexType = m_IndexBuffer->GetIndexType();
        if (m_IndexBuffer)
        {
            if (data->GetIndexType() != indexType)
            {
                CW_ENGINE_ERROR("Invalid index type provided");
                return;
            }

            uint32_t idxSize =
              m_IndexBuffer->GetIndexType() == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t);
            uint8_t* indicies = data->GetIndexData<uint8_t>();
            uint32_t indexCountToCopy = std::min(m_NumIndices, data->GetIndexCount());
            uint32_t indiciesSize = indexCountToCopy * idxSize;
            if (indiciesSize > data->GetIndexBufferSize())
            {
                CW_ENGINE_ERROR("Not enough memory for index buffer");
                return;
            }
            m_IndexBuffer->ReadData(0, m_IndexBuffer->GetBufferSize(), indicies);
        }

        if (m_VertexBuffer)
        {
            const BufferLayout& layout = m_Layout;
            uint32_t vertexSize = layout.GetStride();
            uint32_t dataVertexSize = data->GetBufferLayout().GetStride();
            if (vertexSize != dataVertexSize)
            {
                CW_ENGINE_ERROR("Mismatched layouts");
                return;
            }

            uint32_t vertexCountToCopy = data->GetVertexCount();
            uint32_t bufferSize = m_VertexBuffer->GetLayout().GetStride() * vertexCountToCopy;
            if (bufferSize > m_VertexBuffer->GetBufferSize()) // TODO: Remove the 0 check when buffers are good
            {
                CW_ENGINE_ERROR("Not enough buffer");
                return;
            }

            uint8_t* dst = data->GetVerexBufferData();
            m_VertexBuffer->ReadData(0, m_VertexBuffer->GetBufferSize(), dst);
        }
    }

    void Mesh::UpdateCpuBuffer(const MeshData& meshData)
    {
        if (!m_Usage.IsSet(MeshUsage::CpuCached))
            return;
        if (meshData.GetIndexCount() != m_NumIndices || meshData.GetVertexCount() != m_NumVertices ||
            meshData.GetIndexType() != m_IndexType || meshData.GetBufferLayout().GetStride() != m_Layout.GetStride())
        {
            CW_ENGINE_ERROR("CPU buffer layout mismatch");
            return;
        }

        if (meshData.GetVertexBufferSize() + meshData.GetIndexBufferSize() !=
            m_CPUMeshData->GetVertexBufferSize() + m_CPUMeshData->GetIndexBufferSize())
        {
            CW_ENGINE_ERROR("Size buffer mismatch");
            return;
        }

        std::memcpy(m_CPUMeshData->GetIndexData(), meshData.GetIndexData(),
                    meshData.GetVertexBufferSize() + meshData.GetIndexBufferSize());
    }

    Ref<MeshData> MeshData::Combine(const Vector<Ref<MeshData>>& meshes, const Vector<Vector<SubMesh>>& subMeshes,
                                    Vector<SubMesh>& outSubMeshes)
    {
        if (meshes.size() == 0)
        {
            return nullptr;
        }
        uint32_t totalVertexCount = 0;
        uint32_t totalIndexCount = 0;
        for (const auto& meshData : meshes)
        {
            totalVertexCount += meshData->GetVertexCount();
            totalIndexCount += meshData->GetIndexCount();
        }
        BufferLayout combinedVertexLayout = meshes[0]->GetBufferLayout();
        for (const auto& meshData : meshes)
        {
            const Vector<BufferElement>& meshAttributes = meshData->GetBufferLayout().GetElements();
            for (const BufferElement& element : meshAttributes)
            {
                // TODO: Make sure these match
                // if ()
            }
        }
        Ref<MeshData> combinedMeshData =
          MeshData::Create(totalVertexCount, totalIndexCount, meshes[0]->GetBufferLayout(), meshes[0]->GetIndexType());
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        uint32_t* idxPtr = combinedMeshData->GetIndexData<uint32_t>();
        for (const auto& meshData : meshes)
        {
            uint32_t numIndices = meshData->GetIndexCount();
            uint32_t* srcData = meshData->GetIndexData<uint32_t>();
            for (uint32_t j = 0; j < numIndices; j++)
                idxPtr[j] = srcData[j] + vertexOffset;
            indexOffset += numIndices;
            idxPtr += numIndices;
            vertexOffset += meshData->GetVertexCount();
        }

        uint32_t meshIdx = 0;
        indexOffset = 0;
        /*
        for (const auto& meshData : meshes)
        {
            uint32_t indexCount = meshData->GetIndexCount();
            const Vector<SubMesh> &curSubMeshes = subMeshes[meshIdx];
            for (const auto& subMesh : curSubMeshes)
                outSubMeshes.push_back(SubMesh(subMesh.IndexOffset + indexOffset, subMesh.IndexCount,
        subMesh.MeshDrawMode));

            indexOffset += indexCount;
            meshIdx++;
        } */

        vertexOffset = 0;
        for (const auto& meshData : meshes)
        {
            for (const BufferElement& element : combinedVertexLayout)
            {
                uint32_t dstVertexStride = meshes[0]->GetBufferLayout().GetStride();
                uint8_t* dstData = combinedMeshData->GetElementData(element);
                dstData += vertexOffset * dstVertexStride;
                uint32_t srcVertexCount = meshData->GetVertexCount();
                uint32_t vertexSize = element.Size;
                // if (meshData->GetBufferLayout().HasElement(element.Attribute))
                if (true)
                {
                    uint32_t srcVertexStride = meshData->GetBufferLayout().GetStride();
                    uint8_t* srcData = meshData->GetElementData(element);

                    for (uint32_t i = 0; i < srcVertexCount; i++)
                    {
                        std::memcpy(dstData, srcData, vertexSize);
                        dstData += dstVertexStride;
                        srcData += srcVertexStride;
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < srcVertexCount; i++)
                    {
                        std::memset(dstData, 0, vertexSize);
                        dstData += dstVertexStride;
                    }
                }
            }
            vertexOffset += meshData->GetVertexCount();
        }

        return combinedMeshData;
    }

    Mesh::~Mesh()
    {
        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;
        m_CPUMeshData = nullptr;
    }

    void Mesh::Init()
    {
        const Ref<MeshData>& meshData = m_CPUMeshData ? m_CPUMeshData : m_InitialData;
        const bool isDynamic = m_Usage == MeshUsage::Dynamic;
        const BufferUsage bufferUsage = isDynamic ? BufferUsage::DYNAMIC_DRAW : BufferUsage::STATIC_DRAW;

        m_IndexBuffer = IndexBuffer::Create(m_NumIndices, m_IndexType, bufferUsage);
        m_VertexBuffer = VertexBuffer::Create(m_NumVertices * m_Layout.GetStride(), bufferUsage);
        if (!meshData)
            return;
        WriteData(meshData, isDynamic);

        if (meshData != nullptr)
            meshData->CalculateBounds(m_AABox, m_SphereBounds);
        if (m_Usage.IsSet(MeshUsage::CpuCached) != 0 && m_CPUMeshData == nullptr)
            m_CPUMeshData = AllocBuffer();
    }

    Ref<MeshData> Mesh::AllocBuffer() const
    {
        Ref<MeshData> meshData = CreateRef<MeshData>(m_NumVertices, m_NumIndices, m_Layout, m_IndexType);

        return meshData;
    }

    Ref<Mesh> Mesh::Create(const Ref<MeshData>& meshData, const Vector<SubMesh>& subMeshes, MeshUsageFlags usage,
                           DrawMode drawMode)
    {
        return CreateRef<Mesh>(meshData, subMeshes, usage, drawMode);
    }

    Ref<Mesh> Mesh::Create(uint32_t vertexCount, uint32_t indexCount, const BufferLayout& layout, MeshUsageFlags usage,
                           DrawMode drawMode, IndexType indexType)
    {
        const Vector<SubMesh> subMeshes = { SubMesh(0, indexCount, drawMode) };
        return CreateRef<Mesh>(subMeshes, vertexCount, indexCount, layout, usage, drawMode, indexType);
    }

} // namespace Crowny