#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <atomic>
#include <memory>

namespace Crowny
{
    /** Single-producer/single-consumer result shared across the simulation and render threads. */
    class MeshUploadResult
    {
    public:
        void ResetForSubmission();
        void Publish(Ref<Mesh> mesh);
        bool TryConsume(Ref<Mesh>& mesh);

    private:
        Ref<Mesh> m_Mesh;
        std::atomic<bool> m_Complete{ false };
    };

    /** Owning render-thread command for creating or updating a dynamic mesh. */
    class MeshUploadCommand
    {
    public:
        MeshUploadCommand(Ref<Mesh> existingMesh, MeshDesc description, std::shared_ptr<MeshUploadResult> result);

        MeshUploadCommand(const MeshUploadCommand&) = delete;
        MeshUploadCommand& operator=(const MeshUploadCommand&) = delete;
        MeshUploadCommand(MeshUploadCommand&&) = default;
        MeshUploadCommand& operator=(MeshUploadCommand&&) = default;

        void Execute();

    private:
        Ref<Mesh> m_ExistingMesh;
        MeshDesc m_Description;
        std::shared_ptr<MeshUploadResult> m_Result;
    };
} // namespace Crowny
