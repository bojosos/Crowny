#include "cwpch.h"

#include "Crowny/Renderer/MeshUploadCommand.h"

namespace Crowny
{
    void MeshUploadResult::ResetForSubmission()
    {
        m_Mesh = nullptr;
        m_Complete.store(false, std::memory_order_relaxed);
    }

    void MeshUploadResult::Publish(Ref<Mesh> mesh)
    {
        m_Mesh = std::move(mesh);
        m_Complete.store(true, std::memory_order_release);
    }

    bool MeshUploadResult::TryConsume(Ref<Mesh>& mesh)
    {
        if (!m_Complete.load(std::memory_order_acquire))
            return false;
        mesh = std::move(m_Mesh);
        m_Complete.store(false, std::memory_order_relaxed);
        return true;
    }

    MeshUploadCommand::MeshUploadCommand(Ref<Mesh> existingMesh, MeshDesc description, std::shared_ptr<MeshUploadResult> result)
      : m_ExistingMesh(std::move(existingMesh)), m_Description(std::move(description)), m_Result(std::move(result))
    {
        CW_ENGINE_ASSERT(m_Result != nullptr);
    }

    void MeshUploadCommand::Execute()
    {
        Ref<Mesh> mesh = m_ExistingMesh;
        if (mesh)
        {
            mesh->SetMeshData(m_Description.Data);
            mesh->UploadToGpu();
        }
        else
        {
            mesh = Mesh::Create(m_Description);
        }
        m_Result->Publish(std::move(mesh));
    }
} // namespace Crowny
