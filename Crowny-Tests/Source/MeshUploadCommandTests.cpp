#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/FrameContext.h"

#include <array>
#include <iterator>
#include <memory>
#include <type_traits>

using namespace Crowny;

TEST_CASE("Mesh upload commands retain their inputs until the frame releases them", "[Renderer][Resources][MeshUpload]")
{
    static_assert(!std::is_copy_constructible_v<MeshUploadCommand>);
    static_assert(std::is_move_constructible_v<MeshUploadCommand>);

    Ref<MeshData> meshData = CreateRef<MeshData>();
    MeshData* meshDataPointer = meshData.get();
    std::shared_ptr<MeshUploadResult> result = std::make_shared<MeshUploadResult>();
    const std::weak_ptr<MeshUploadResult> resultLifetime = result;

    MeshDesc description;
    description.Data = meshData;
    FrameContext frame;
    frame.MeshUploadCommands.emplace_back(nullptr, std::move(description), result);

    meshData = nullptr;
    result.reset();
    CHECK(meshDataPointer->GetRefCount() == 1u);
    CHECK_FALSE(resultLifetime.expired());

    frame.MeshUploadCommands.clear();
    CHECK(resultLifetime.expired());
}

TEST_CASE("Mesh upload results publish once across the thread handoff", "[Renderer][Resources][MeshUpload]")
{
    MeshUploadResult result;
    Ref<Mesh> uploadedMesh;

    result.ResetForSubmission();
    CHECK_FALSE(result.TryConsume(uploadedMesh));
    result.Publish(nullptr);
    CHECK(result.TryConsume(uploadedMesh));
    CHECK_FALSE(result.TryConsume(uploadedMesh));
}

TEST_CASE("Mesh upload command storage allocates nothing after warm-up", "[Renderer][Resources][MeshUpload][Memory][Frame]")
{
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0
    SKIP("MSVC debug iterators allocate bookkeeping storage during vector moves.");
#endif

    constexpr std::array<uint32_t, 3> commandCounts{ 1u, 1000u, 10000u };
    constexpr uint32_t frameCount = 120u;

    for (const uint32_t commandCount : commandCounts)
    {
        Ref<MeshData> meshData = CreateRef<MeshData>();
        const std::shared_ptr<MeshUploadResult> result = std::make_shared<MeshUploadResult>();
        Vector<MeshUploadCommand> pendingCommands;
        FrameContext frame;

        auto transferCommands = [&]() {
            for (uint32_t commandIndex = 0; commandIndex < commandCount; commandIndex++)
            {
                MeshDesc description;
                description.Data = meshData;
                pendingCommands.emplace_back(nullptr, std::move(description), result);
            }
            frame.MeshUploadCommands.insert(frame.MeshUploadCommands.end(), std::make_move_iterator(pendingCommands.begin()),
                                            std::make_move_iterator(pendingCommands.end()));
            pendingCommands.clear();
            frame.MeshUploadCommands.clear();
        };

        transferCommands();
        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        for (uint32_t frameIndex = 0; frameIndex < frameCount; frameIndex++)
            transferCommands();
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        INFO("Command count: " << commandCount);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}
