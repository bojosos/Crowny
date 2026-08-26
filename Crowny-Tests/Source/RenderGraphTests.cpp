#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/GpuBufferPool.h"
#include "Crowny/Renderer/GpuTexturePool.h"
#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderGraphResources.h"

#include <array>

using namespace Crowny;

namespace
{
    RenderGraphTextureDesc ColorTexture()
    {
        RenderGraphTextureDesc desc;
        desc.Width = 1920;
        desc.Height = 1080;
        desc.Format = TextureFormat::RGBA16F;
        return desc;
    }

    class RecordingGraphListener final : public IRenderGraphExecutionListener
    {
    public:
        void ApplyBarrier(const RenderGraphBarrier& barrier) override { Events.push_back(barrier.AfterPass ? "BarrierAfter" : "BarrierBefore"); }
        void BeginPass(RenderGraphPassHandle, StringView, RenderGraphQueue) override { Events.push_back("BeginPass"); }
        void EndPass(RenderGraphPassHandle, StringView, RenderGraphQueue) override { Events.push_back("EndPass"); }

        Vector<String> Events;
    };

    class RecordingResourceAllocator final : public IRenderGraphResourceAllocator
    {
    public:
        Ref<Texture> CreateTexture(StringView name, const RenderGraphTextureDesc& desc) override
        {
            TextureNames.emplace_back(name);
            TextureDescs.push_back(desc);
            return nullptr;
        }

        Ref<GenericGpuBuffer> CreateBuffer(StringView name, const RenderGraphBufferDesc& desc) override
        {
            BufferNames.emplace_back(name);
            BufferDescs.push_back(desc);
            return nullptr;
        }

        Vector<String> TextureNames;
        Vector<RenderGraphTextureDesc> TextureDescs;
        Vector<String> BufferNames;
        Vector<RenderGraphBufferDesc> BufferDescs;
    };

    class TestGpuBuffer final : public GenericGpuBuffer
    {
    public:
        void* Map(uint32_t, uint32_t, GpuLockOptions) override { return nullptr; }
        void Unmap() override {}
        uint32_t GetBufferSize() const override { return 64; }
        void WriteData(uint32_t, uint32_t, const void*, BufferWriteOptions) override {}
        void ReadData(uint32_t, uint32_t, void*) override {}
    };

    class TestGpuTexture final : public Texture
    {
    public:
        explicit TestGpuTexture(const TextureDesc& desc) : Texture(desc, true) {}

        PixelData Lock(GpuLockOptions, uint32_t, uint32_t, uint32_t) override { return {}; }
        void Unlock() override {}
        void ReadData(PixelData&, uint32_t, uint32_t, uint32_t) override {}
        bool ReadPixel(uint32_t, uint32_t, void*, size_t, uint32_t, uint32_t, uint32_t) override { return false; }
        void WriteData(const PixelData&, uint32_t, uint32_t, uint32_t) override {}
    };

    TextureDesc PooledTextureDesc()
    {
        TextureDesc desc;
        desc.Type = TextureType::TEXTURE_DEFAULT;
        desc.Shape = TextureShape::TEXTURE_2D;
        desc.sRGB = false;
        desc.Width = 16;
        desc.Height = 8;
        desc.Depth = 1;
        desc.MipLevels = 0;
        desc.Samples = 1;
        desc.Faces = 1;
        desc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        desc.Format = TextureFormat::RGBA8;
        return desc;
    }

    class RetiringTextureAllocator final : public IRenderGraphResourceAllocator
    {
    public:
        Ref<Texture> CreateTexture(StringView, const RenderGraphTextureDesc& desc) override
        {
            TextureDesc textureDesc;
            textureDesc.Shape = desc.Shape;
            textureDesc.sRGB = false;
            textureDesc.Width = std::max(desc.Width, 1u);
            textureDesc.Height = std::max(desc.Height, 1u);
            textureDesc.Depth = std::max(desc.Depth, 1u);
            textureDesc.MipLevels = std::max(desc.MipLevels, 1u) - 1u;
            textureDesc.Samples = std::max(desc.Samples, 1u);
            textureDesc.Faces = std::max(desc.Layers, 1u);
            textureDesc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
            textureDesc.Format = desc.Format;
            Ref<TestGpuTexture> texture = CreateRef<TestGpuTexture>(textureDesc);
            LastCreated = texture.get();
            return texture;
        }

        Ref<GenericGpuBuffer> CreateBuffer(StringView, const RenderGraphBufferDesc&) override { return nullptr; }

        void ReleaseTexture(const RenderGraphTextureDesc& desc, Ref<Texture>&& texture) override
        {
            ReleasedDescs.push_back(desc);
            ReleasedTextures.push_back(texture.get());
            ReleasedResources.push_back(std::move(texture));
        }

        Texture* LastCreated = nullptr;
        Vector<RenderGraphTextureDesc> ReleasedDescs;
        Vector<Texture*> ReleasedTextures;
        Vector<Ref<Texture>> ReleasedResources;
    };

    class TestRenderTarget final : public RenderTarget
    {
    public:
        void Resize(uint32_t width, uint32_t height) override
        {
            m_Properties.Width = width;
            m_Properties.Height = height;
        }
        const RenderTargetProperties& GetProperties() const override { return m_Properties; }
        void SwapBuffers(uint32_t) override {}

    private:
        RenderTargetProperties m_Properties;
    };
} // namespace

TEST_CASE("RenderGraph rebuild and compile reuse warm scratch storage", "[Memory][Frame][Renderer][RenderGraph]")
{
    RenderGraph graph;
    bool compiledSuccessfully = true;
    auto rebuild = [&](bool fullTopology) {
        graph.Reset();
        const RenderGraphResourceHandle uploadBuffer =
          graph.CreateBuffer("RenderGraphAllocationUploadBuffer", { 4096, 16, GpuBufferType::Structured });
        const RenderGraphResourceHandle color = graph.CreateTexture("RenderGraphAllocationColorTexture", ColorTexture());

        const std::array<uint64_t, 8> oversizedCapture{};
        const auto uploadSetup = [uploadBuffer, oversizedCapture](RenderGraphPassBuilder& builder) {
            static_cast<void>(oversizedCapture);
            builder.Write(uploadBuffer, RenderGraphResourceState::TransferWrite);
        };
        static_assert(sizeof(uploadSetup) > 32u);
        graph.AddPass("RenderGraphAllocationUploadPass", RenderGraphQueue::Transfer, uploadSetup);
        graph.AddPass("RenderGraphAllocationCullPass", RenderGraphQueue::Compute,
                      [uploadBuffer](RenderGraphPassBuilder& builder) { builder.Read(uploadBuffer, RenderGraphResourceState::ShaderRead); });
        graph.AddPass("RenderGraphAllocationColorPass", RenderGraphQueue::Graphics,
                      [color](RenderGraphPassBuilder& builder) { builder.Write(color, RenderGraphResourceState::ColorAttachment); });
        graph.AddPass("RenderGraphAllocationPresentPass", RenderGraphQueue::Graphics,
                      [color](RenderGraphPassBuilder& builder) { builder.Read(color, RenderGraphResourceState::ShaderRead); });

        if (fullTopology)
        {
            const RenderGraphResourceHandle visibleBuffer =
              graph.CreateBuffer("RenderGraphAllocationVisibleBuffer", { 4096, 16, GpuBufferType::Structured });
            const RenderGraphHistoryPair history = graph.CreateHistoryTexture("RenderGraphAllocationHistoryTexture", ColorTexture());
            graph.AddPass("RenderGraphAllocationVisibilityPass", RenderGraphQueue::Compute,
                          [visibleBuffer](RenderGraphPassBuilder& builder) { builder.Write(visibleBuffer, RenderGraphResourceState::ShaderWrite); });
            graph.AddPass("RenderGraphAllocationHistoryReadPass", RenderGraphQueue::Compute,
                          [history](RenderGraphPassBuilder& builder) { builder.Read(history.Read, RenderGraphResourceState::ShaderRead); });
            graph.AddPass("RenderGraphAllocationHistoryWritePass", RenderGraphQueue::Compute,
                          [history](RenderGraphPassBuilder& builder) { builder.Write(history.Write, RenderGraphResourceState::ShaderWrite); });
        }

        const RenderGraphCompileResult& compiled = graph.Compile();
        const size_t expectedPasses = fullTopology ? 7 : 4;
        const size_t expectedResources = fullTopology ? 5 : 2;
        compiledSuccessfully &= compiled.Succeeded && compiled.PassOrder.size() == expectedPasses && compiled.Resources.size() == expectedResources;
    };

    rebuild(true);
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
        rebuild((frame & 1u) == 0);
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();

    CHECK(compiledSuccessfully);
    CHECK(Memory::GetThreadAllocationDelta(before, after).AllocationCount == 0);
    CHECK(Memory::GetThreadAllocationDelta(before, after).RequestedBytes == 0);
}

TEST_CASE("RenderGraph clears retained callbacks and compiler state after failures", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    std::shared_ptr<int> callbackOwner = std::make_shared<int>(42);
    const std::weak_ptr<int> callbackLifetime = callbackOwner;
    graph.AddPass("RetainedCallback", RenderGraphQueue::Graphics, {}, [callbackOwner](RenderGraphContext&) {});
    callbackOwner.reset();
    REQUIRE(graph.Compile().Succeeded);
    CHECK_FALSE(callbackLifetime.expired());

    graph.Reset();
    CHECK(callbackLifetime.expired());
    const RenderGraphResourceHandle invalidRead = graph.CreateTexture("FailureResource", ColorTexture());
    graph.AddPass("InvalidRead", RenderGraphQueue::Graphics,
                  [invalidRead](RenderGraphPassBuilder& builder) { builder.Read(invalidRead, RenderGraphResourceState::ShaderRead); });
    REQUIRE_FALSE(graph.Compile().Succeeded);

    graph.Reset();
    const RenderGraphResourceHandle validWrite = graph.CreateTexture("RecoveredResource", ColorTexture());
    graph.AddPass("ValidWrite", RenderGraphQueue::Graphics,
                  [validWrite](RenderGraphPassBuilder& builder) { builder.Write(validWrite, RenderGraphResourceState::ColorAttachment); });
    const RenderGraphCompileResult& recovered = graph.Compile();
    REQUIRE(recovered.Succeeded);
    CHECK(recovered.Error.empty());
    CHECK(recovered.PassOrder.size() == 1);
    CHECK(recovered.Resources.size() == 1);
}

TEST_CASE("RenderGraph orders resource hazards", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle color = graph.CreateTexture("Color", ColorTexture());

    const RenderGraphPassHandle produce = graph.AddPass("Produce", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Write(color, RenderGraphResourceState::ColorAttachment);
    });
    const RenderGraphPassHandle consume = graph.AddPass(
      "Consume", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Read(color, RenderGraphResourceState::ShaderRead); });
    const RenderGraphPassHandle overwrite = graph.AddPass(
      "Overwrite", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Write(color, RenderGraphResourceState::ShaderWrite); });

    const RenderGraphCompileResult& result = graph.Compile();
    REQUIRE(result.Succeeded);
    REQUIRE(result.PassOrder.size() == 3);
    CHECK(result.PassOrder[0] == produce);
    CHECK(result.PassOrder[1] == consume);
    CHECK(result.PassOrder[2] == overwrite);
    CHECK(result.Barriers.size() == 3);
}

TEST_CASE("RenderGraph rejects a transient read before its first write", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle color = graph.CreateTexture("Uninitialized", ColorTexture());
    graph.AddPass("Read", RenderGraphQueue::Graphics,
                  [&](RenderGraphPassBuilder& builder) { builder.Read(color, RenderGraphResourceState::ShaderRead); });

    const RenderGraphCompileResult& result = graph.Compile();
    CHECK_FALSE(result.Succeeded);
    CHECK(result.Error.find("before its first write") != String::npos);
}

TEST_CASE("RenderGraph detects explicit dependency cycles", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphPassHandle first = graph.AddPass("First", RenderGraphQueue::Graphics, {});
    const RenderGraphPassHandle second = graph.AddPass("Second", RenderGraphQueue::Graphics, {});
    graph.AddDependency(first, second);
    graph.AddDependency(second, first);

    const RenderGraphCompileResult& result = graph.Compile();
    CHECK_FALSE(result.Succeeded);
    CHECK(result.Error.find("cycle") != String::npos);
}

TEST_CASE("RenderGraph aliases compatible non-overlapping transients", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle first = graph.CreateTexture("First", ColorTexture());
    const RenderGraphResourceHandle second = graph.CreateTexture("Second", ColorTexture());

    const RenderGraphPassHandle firstWrite = graph.AddPass("FirstWrite", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Write(first, RenderGraphResourceState::ColorAttachment);
    });
    const RenderGraphPassHandle firstRead = graph.AddPass(
      "FirstRead", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) { builder.Read(first, RenderGraphResourceState::ShaderRead); });
    const RenderGraphPassHandle secondWrite = graph.AddPass("SecondWrite", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.DependsOn(firstRead);
        builder.Write(second, RenderGraphResourceState::ColorAttachment);
    });
    static_cast<void>(firstWrite);
    static_cast<void>(secondWrite);

    const RenderGraphCompileResult& result = graph.Compile();
    REQUIRE(result.Succeeded);
    CHECK(result.Resources[first.Index].LastUse < result.Resources[second.Index].FirstUse);
    CHECK(result.Resources[first.Index].PhysicalIndex == result.Resources[second.Index].PhysicalIndex);
    CHECK(result.PhysicalTextureCount == 1);
    CHECK(result.TransientTextureBytes == 1920ull * 1080ull * 8ull);
}

TEST_CASE("RenderGraph keeps overlapping resources in separate allocations", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle first = graph.CreateTexture("First", ColorTexture());
    const RenderGraphResourceHandle second = graph.CreateTexture("Second", ColorTexture());

    graph.AddPass("WriteBoth", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Write(first, RenderGraphResourceState::ColorAttachment);
        builder.Write(second, RenderGraphResourceState::ColorAttachment);
    });
    graph.AddPass("ReadBoth", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) {
        builder.Read(first, RenderGraphResourceState::ShaderRead);
        builder.Read(second, RenderGraphResourceState::ShaderRead);
    });

    const RenderGraphCompileResult& result = graph.Compile();
    REQUIRE(result.Succeeded);
    CHECK(result.Resources[first.Index].PhysicalIndex != result.Resources[second.Index].PhysicalIndex);
    CHECK(result.PhysicalTextureCount == 2);
}

TEST_CASE("RenderGraph executes callbacks in compiled order", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle buffer = graph.CreateBuffer("Buffer", { 256, 16, GpuBufferType::Structured });
    Vector<String> executionOrder;

    graph.AddPass(
      "Upload", RenderGraphQueue::Transfer, [&](RenderGraphPassBuilder& builder) { builder.Write(buffer, RenderGraphResourceState::TransferWrite); },
      [&](RenderGraphContext&) { executionOrder.push_back("Upload"); });
    graph.AddPass(
      "Consume", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Read(buffer, RenderGraphResourceState::ShaderRead); },
      [&](RenderGraphContext&) { executionOrder.push_back("Consume"); });

    REQUIRE(graph.Execute());
    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == "Upload");
    CHECK(executionOrder[1] == "Consume");
    CHECK(graph.GetExecutionStats().ExecutedPasses == 2);
    CHECK(graph.GetExecutionStats().ExecutedCallbacks == 2);
    CHECK(graph.GetExecutionStats().ComputePasses == 1);
    CHECK(graph.GetExecutionStats().TransferPasses == 1);
    CHECK(graph.GetExecutionStats().Succeeded);
}

TEST_CASE("RenderGraph history pairs share a stable identity", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphHistoryPair history = graph.CreateHistoryTexture("CameraHiZ", ColorTexture());
    graph.AddPass("ReadHistory", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Read(history.Read); });
    graph.AddPass("WriteHistory", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Write(history.Write); });

    const RenderGraphCompileResult& result = graph.Compile();
    REQUIRE(result.Succeeded);
    const RenderGraphResourceDesc& read = result.Resources[history.Read.Index].Desc;
    const RenderGraphResourceDesc& write = result.Resources[history.Write.Index].Desc;
    CHECK(read.HistoryId != 0);
    CHECK(read.HistoryId == write.HistoryId);
    CHECK(read.HistoryRole == RenderGraphHistoryRole::Read);
    CHECK(write.HistoryRole == RenderGraphHistoryRole::Write);
}

TEST_CASE("RenderGraph execution emits final transitions after the last pass", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle output =
      graph.ImportTexture("Output", ColorTexture(), 7, RenderGraphResourceState::ShaderRead, RenderGraphResourceState::Present);
    graph.AddPass("Draw", RenderGraphQueue::Graphics,
                  [&](RenderGraphPassBuilder& builder) { builder.Write(output, RenderGraphResourceState::ColorAttachment); });

    RecordingGraphListener listener;
    REQUIRE(graph.Execute(&listener));
    const Vector<String> expected{ "BarrierBefore", "BeginPass", "EndPass", "BarrierAfter" };
    CHECK(listener.Events == expected);
    CHECK(graph.GetExecutionStats().ScheduledBarriers == 2);
    CHECK(graph.GetExecutionStats().AppliedBarriers == 2);
}

TEST_CASE("RenderGraph history ping-pongs per camera and resets on cuts", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphHistoryPair history = graph.CreateHistoryTexture("Taa", ColorTexture());
    graph.AddPass("Resolve", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) {
        builder.Read(history.Read);
        builder.Write(history.Write);
    });
    const RenderGraphCompileResult& compiled = graph.Compile();
    REQUIRE(compiled.Succeeded);

    RenderGraphResourceRegistry resources(2);
    REQUIRE(resources.BeginFrame(compiled, 1, 42, true));
    const RenderGraphResourceBinding firstRead = resources.Get(history.Read);
    const RenderGraphResourceBinding firstWrite = resources.Get(history.Write);
    CHECK_FALSE(firstRead.HistoryValid);
    CHECK(firstRead.PhysicalId != firstWrite.PhysicalId);
    resources.EndFrame();

    REQUIRE(resources.BeginFrame(compiled, 2, 42));
    const RenderGraphResourceBinding secondRead = resources.Get(history.Read);
    const RenderGraphResourceBinding secondWrite = resources.Get(history.Write);
    CHECK(secondRead.HistoryValid);
    CHECK(secondRead.PhysicalId == firstWrite.PhysicalId);
    CHECK(secondWrite.PhysicalId == firstRead.PhysicalId);
    resources.EndFrame();

    REQUIRE(resources.BeginFrame(compiled, 3, 7));
    CHECK_FALSE(resources.Get(history.Read).HistoryValid);
    CHECK(resources.Get(history.Read).PhysicalId != secondRead.PhysicalId);
    resources.EndFrame();

    REQUIRE(resources.BeginFrame(compiled, 4, 42, true));
    CHECK_FALSE(resources.Get(history.Read).HistoryValid);
    resources.EndFrame();
}

TEST_CASE("RenderGraph registry preserves transient aliasing within a frame", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle first = graph.CreateTexture("First", ColorTexture());
    const RenderGraphResourceHandle second = graph.CreateTexture("Second", ColorTexture());
    const RenderGraphPassHandle firstRead = graph.AddPass("FirstWrite", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Write(first, RenderGraphResourceState::ColorAttachment);
    });
    graph.AddPass("SecondWrite", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.DependsOn(firstRead);
        builder.Write(second, RenderGraphResourceState::ColorAttachment);
    });
    const RenderGraphCompileResult& compiled = graph.Compile();
    REQUIRE(compiled.Succeeded);
    REQUIRE(compiled.Resources[first.Index].PhysicalIndex == compiled.Resources[second.Index].PhysicalIndex);

    RenderGraphResourceRegistry resources;
    REQUIRE(resources.BeginFrame(compiled, 1, 1));
    CHECK(resources.Get(first).PhysicalId == resources.Get(second).PhysicalId);
    const RenderGraphResourceRegistryStats firstFrameStats = resources.GetStats();
    CHECK(firstFrameStats.FramePhysicalScratchCapacity >= 1);
    CHECK(firstFrameStats.FramePhysicalScratchGrowths == 1);
    resources.EndFrame();

    REQUIRE(resources.BeginFrame(compiled, 3, 1));
    CHECK(resources.Get(first).PhysicalId == resources.Get(second).PhysicalId);
    CHECK(resources.GetStats().FramePhysicalScratchCapacity == firstFrameStats.FramePhysicalScratchCapacity);
    CHECK(resources.GetStats().FramePhysicalScratchGrowths == firstFrameStats.FramePhysicalScratchGrowths);
    resources.EndFrame();
}

TEST_CASE("RenderGraph resources allocate lazily and preserve physical aliasing", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    RenderGraphTextureDesc textureDesc = ColorTexture();
    textureDesc.MipLevels = 5;
    const RenderGraphResourceHandle first = graph.CreateTexture("First", textureDesc);
    const RenderGraphResourceHandle second = graph.CreateTexture("Second", textureDesc);
    const RenderGraphPassHandle firstPass = graph.AddPass(
      "FirstPass", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Write(first); },
      [&](RenderGraphContext& context) { CHECK_FALSE(context.GetTexture(first)); });
    graph.AddPass(
      "SecondPass", RenderGraphQueue::Compute,
      [&](RenderGraphPassBuilder& builder) {
          builder.DependsOn(firstPass);
          builder.Write(second);
      },
      [&](RenderGraphContext& context) { CHECK_FALSE(context.GetTexture(second)); });

    const RenderGraphCompileResult& compiled = graph.Compile();
    REQUIRE(compiled.Succeeded);
    REQUIRE(compiled.Resources[first.Index].PhysicalIndex == compiled.Resources[second.Index].PhysicalIndex);

    RecordingResourceAllocator allocator;
    RenderGraphResourceRegistry resources(2, &allocator);
    REQUIRE(resources.BeginFrame(compiled, 1, 1));
    CHECK(allocator.TextureNames.empty());
    CHECK_FALSE(graph.Execute(nullptr, &resources));
    REQUIRE(allocator.TextureNames.size() == 1);
    CHECK(allocator.TextureNames[0] == "First");
    CHECK(allocator.TextureDescs[0] == textureDesc);
    CHECK(resources.Get(first).PhysicalId == resources.Get(second).PhysicalId);
    CHECK(resources.GetStats().AllocationFailures == 1);
    CHECK(graph.GetExecutionStats().ExecutedPasses == 1);
    CHECK_FALSE(graph.GetExecutionStats().Succeeded);
    resources.EndFrame();

    REQUIRE(resources.BeginFrame(compiled, 3, 1));
    CHECK_FALSE(graph.Execute(nullptr, &resources));
    CHECK(allocator.TextureNames.size() == 1);
    CHECK(resources.HasAllocationFailure());
    resources.EndFrame();
}

TEST_CASE("RenderGraph context resolves typed buffer resources", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphBufferDesc bufferDesc{ 1024, 16, GpuBufferType::Structured };
    const RenderGraphResourceHandle buffer = graph.CreateBuffer("Commands", bufferDesc);
    graph.AddPass(
      "WriteCommands", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) { builder.Write(buffer); },
      [&](RenderGraphContext& context) { CHECK_FALSE(context.GetBuffer(buffer)); });

    const RenderGraphCompileResult& compiled = graph.Compile();
    REQUIRE(compiled.Succeeded);
    RecordingResourceAllocator allocator;
    RenderGraphResourceRegistry resources(2, &allocator);
    REQUIRE(resources.BeginFrame(compiled, 1, 1));
    CHECK_FALSE(graph.Execute(nullptr, &resources));
    REQUIRE(allocator.BufferNames.size() == 1);
    CHECK(allocator.BufferNames[0] == "Commands");
    CHECK(allocator.BufferDescs[0] == bufferDesc);
    resources.EndFrame();
}

TEST_CASE("RenderGraph binds imported resources to typed runtime objects", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const Ref<TestGpuBuffer> bufferResource = CreateRef<TestGpuBuffer>();
    const Ref<TestRenderTarget> targetResource = CreateRef<TestRenderTarget>();
    const RenderGraphResourceHandle buffer =
      graph.ImportBuffer("Instances", { 64, 16, GpuBufferType::Structured }, reinterpret_cast<uint64_t>(bufferResource.get()),
                         RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
    const RenderGraphResourceHandle target =
      graph.ImportTexture("Output", ColorTexture(), reinterpret_cast<uint64_t>(targetResource.get()), RenderGraphResourceState::ColorAttachment,
                          RenderGraphResourceState::ColorAttachment);
    graph.AddPass(
      "UseImportedResources", RenderGraphQueue::Graphics,
      [&](RenderGraphPassBuilder& builder) {
          builder.Read(buffer);
          builder.Write(target, RenderGraphResourceState::ColorAttachment);
      },
      [&](RenderGraphContext& context) {
          CHECK(context.GetBuffer(buffer).get() == bufferResource.get());
          CHECK(context.GetRenderTarget(target).get() == targetResource.get());
      });

    const RenderGraphCompileResult& compiled = graph.Compile();
    REQUIRE(compiled.Succeeded);
    RenderGraphResourceRegistry resources;
    REQUIRE(resources.BeginFrame(compiled, 1, 1));
    REQUIRE(resources.BindExternalBuffer(buffer, bufferResource));
    REQUIRE(resources.BindExternalRenderTarget(target, targetResource));
    CHECK(graph.Execute(nullptr, &resources));
    resources.EndFrame();
}

TEST_CASE("GpuBufferPool delays whole-buffer reuse until in-flight frames retire", "[Renderer][Resources]")
{
    GpuBufferPool pool(2, 1024);
    GenericGpuBufferDesc desc;
    desc.ElementCount = 4;
    desc.ElementSize = 16;
    desc.Type = GpuBufferType::Structured;
    desc.Usage = BufferUsage::BU_LOADSTORE;

    Ref<TestGpuBuffer> concrete = CreateRef<TestGpuBuffer>();
    TestGpuBuffer* identity = concrete.get();
    Ref<GenericGpuBuffer> buffer = concrete;
    concrete = nullptr;

    pool.BeginFrame(10);
    pool.Release(desc, std::move(buffer));
    CHECK(pool.GetStats().RetiredBuffers == 1);

    pool.BeginFrame(12);
    Ref<GenericGpuBuffer> reused = pool.Acquire(desc);
    REQUIRE(reused);
    CHECK(reused.get() == identity);
    CHECK(pool.GetStats().Reused == 1);
    CHECK(pool.GetStats().RetainedBytes == 0);
}

TEST_CASE("GpuTexturePool delays reuse and ignores debug names", "[Renderer][Resources]")
{
    GpuTexturePool pool(2, 1024 * 1024);
    TextureDesc desc = PooledTextureDesc();
    desc.DebugName = "FirstLogicalResource";
    Ref<TestGpuTexture> concrete = CreateRef<TestGpuTexture>(desc);
    TestGpuTexture* identity = concrete.get();
    Ref<Texture> texture = concrete;
    concrete = nullptr;

    pool.BeginFrame(10);
    pool.Release(std::move(texture));
    CHECK(pool.GetStats().RetiredTextures == 1);
    CHECK(pool.GetStats().AvailableTextures == 0);

    pool.BeginFrame(11);
    CHECK(pool.GetStats().RetiredTextures == 1);
    pool.BeginFrame(12);
    CHECK(pool.GetStats().RetiredTextures == 0);
    CHECK(pool.GetStats().AvailableTextures == 1);

    desc.DebugName = "SecondLogicalResource";
    Ref<Texture> reused = pool.Acquire(desc);
    REQUIRE(reused);
    CHECK(reused.get() == identity);
    CHECK(pool.GetStats().Reused == 1);
    CHECK(pool.GetStats().RetainedBytes == 0);
}

TEST_CASE("GpuTexturePool separates every resource-creation field", "[Renderer][Resources]")
{
    GpuTexturePool pool(2, 64ull * 1024ull * 1024ull);
    const TextureDesc base = PooledTextureDesc();
    Vector<TextureDesc> descriptors(13, base);
    descriptors[1].Type = TextureType::TEXTURE_SPRITE;
    descriptors[2].Shape = TextureShape::TEXTURE_3D;
    descriptors[3].sRGB = true;
    descriptors[4].ReadWrite = true;
    descriptors[5].GenerateMipmaps = true;
    descriptors[6].MipLevels = 1;
    descriptors[7].Samples = 2;
    descriptors[8].Faces = 2;
    descriptors[9].Width = 32;
    descriptors[10].Height = 16;
    descriptors[11].Depth = 2;
    descriptors[12].Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
    descriptors.push_back(base);
    descriptors.back().Format = TextureFormat::RG16F;

    Vector<TestGpuTexture*> identities;
    identities.reserve(descriptors.size());
    pool.BeginFrame(20);
    for (const TextureDesc& desc : descriptors)
    {
        Ref<TestGpuTexture> concrete = CreateRef<TestGpuTexture>(desc);
        identities.push_back(concrete.get());
        Ref<Texture> texture = concrete;
        concrete = nullptr;
        pool.Release(std::move(texture));
    }

    pool.BeginFrame(22);
    for (size_t index = 0; index < descriptors.size(); index++)
    {
        Ref<Texture> texture = pool.Acquire(descriptors[index]);
        REQUIRE(texture);
        CHECK(texture.get() == identities[index]);
    }
    CHECK(pool.GetStats().Reused == descriptors.size());
    CHECK(pool.GetStats().RetainedBytes == 0);
}

TEST_CASE("GpuTexturePool enforces its retained-byte budget and trims ready textures", "[Renderer][Resources]")
{
    TextureDesc desc = PooledTextureDesc();
    GpuTexturePool rejectedPool(2, 128);
    rejectedPool.BeginFrame(1);
    Ref<Texture> oversized = CreateRef<TestGpuTexture>(desc);
    rejectedPool.Release(std::move(oversized));
    CHECK(rejectedPool.GetStats().Rejected == 1);
    CHECK(rejectedPool.GetStats().RetainedBytes == 0);

    GpuTexturePool trimmedPool(2, 1024);
    trimmedPool.BeginFrame(1);
    Ref<Texture> retained = CreateRef<TestGpuTexture>(desc);
    trimmedPool.Release(std::move(retained));
    trimmedPool.BeginFrame(3);
    CHECK(trimmedPool.GetStats().AvailableTextures == 1);
    trimmedPool.SetRetainedByteBudget(0);
    CHECK(trimmedPool.GetStats().AvailableTextures == 0);
    CHECK(trimmedPool.GetStats().RetainedBytes == 0);
}

TEST_CASE("GpuTexturePool rejects invalid zero-valued descriptors instead of aliasing them", "[Renderer][Resources]")
{
    GpuTexturePool pool(2, 1024 * 1024);
    const TextureDesc valid = PooledTextureDesc();
    Vector<TextureDesc> invalid(5, valid);
    invalid[0].Width = 0;
    invalid[1].Height = 0;
    invalid[2].Depth = 0;
    invalid[3].Samples = 0;
    invalid[4].Faces = 0;

    pool.BeginFrame(30);
    for (const TextureDesc& desc : invalid)
    {
        Ref<Texture> texture = CreateRef<TestGpuTexture>(desc);
        pool.Release(std::move(texture));
    }
    CHECK(pool.GetStats().Rejected == invalid.size());
    CHECK(pool.GetStats().RetiredTextures == 0);
    CHECK(pool.GetStats().RetainedBytes == 0);

    Ref<TestGpuTexture> concrete = CreateRef<TestGpuTexture>(valid);
    TestGpuTexture* identity = concrete.get();
    Ref<Texture> texture = concrete;
    concrete = nullptr;
    pool.Release(std::move(texture));
    pool.BeginFrame(32);

    Ref<Texture> reused = pool.Acquire(valid);
    REQUIRE(reused);
    CHECK(reused.get() == identity);
}

TEST_CASE("RenderGraph retires replaced physical textures through its allocator", "[Renderer][Resources][RenderGraph]")
{
    RenderGraph graph;
    RetiringTextureAllocator allocator;
    RenderGraphResourceRegistry resources(2, &allocator);
    RenderGraphTextureDesc firstDesc = ColorTexture();
    const RenderGraphResourceHandle first = graph.CreateTexture("First", firstDesc);
    graph.AddPass(
      "WriteFirst", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) { builder.Write(first); },
      [&](RenderGraphContext& context) { REQUIRE(context.GetTexture(first)); });
    REQUIRE(graph.Compile().Succeeded);
    REQUIRE(resources.BeginFrame(graph.GetCompileResult(), 1, 1));
    REQUIRE(graph.Execute(nullptr, &resources));
    Texture* firstIdentity = allocator.LastCreated;
    REQUIRE(firstIdentity != nullptr);
    resources.EndFrame();

    graph.Reset();
    RenderGraphTextureDesc secondDesc = firstDesc;
    secondDesc.Width /= 2u;
    const RenderGraphResourceHandle second = graph.CreateTexture("Second", secondDesc);
    graph.AddPass(
      "WriteSecond", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) { builder.Write(second); },
      [&](RenderGraphContext& context) { REQUIRE(context.GetTexture(second)); });
    REQUIRE(graph.Compile().Succeeded);
    REQUIRE(resources.BeginFrame(graph.GetCompileResult(), 3, 1));
    REQUIRE(allocator.ReleasedTextures.size() == 1);
    CHECK(allocator.ReleasedTextures[0] == firstIdentity);
    CHECK(allocator.ReleasedDescs[0] == firstDesc);
    REQUIRE(graph.Execute(nullptr, &resources));
    CHECK(allocator.LastCreated != firstIdentity);
    resources.EndFrame();
}
