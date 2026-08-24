#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderGraphResources.h"
#include "Crowny/Renderer/GpuBufferPool.h"

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
        void ApplyBarrier(const RenderGraphBarrier& barrier) override
        {
            Events.push_back(barrier.AfterPass ? "BarrierAfter" : "BarrierBefore");
        }
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
}

TEST_CASE("RenderGraph orders resource hazards", "[Renderer][RenderGraph]")
{
    RenderGraph graph;
    const RenderGraphResourceHandle color = graph.CreateTexture("Color", ColorTexture());

    const RenderGraphPassHandle produce = graph.AddPass("Produce", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Write(color, RenderGraphResourceState::ColorAttachment);
    });
    const RenderGraphPassHandle consume = graph.AddPass("Consume", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) {
        builder.Read(color, RenderGraphResourceState::ShaderRead);
    });
    const RenderGraphPassHandle overwrite = graph.AddPass("Overwrite", RenderGraphQueue::Compute, [&](RenderGraphPassBuilder& builder) {
        builder.Write(color, RenderGraphResourceState::ShaderWrite);
    });

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
    const RenderGraphPassHandle firstRead = graph.AddPass("FirstRead", RenderGraphQueue::Graphics, [&](RenderGraphPassBuilder& builder) {
        builder.Read(first, RenderGraphResourceState::ShaderRead);
    });
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

    graph.AddPass("Upload", RenderGraphQueue::Transfer,
                  [&](RenderGraphPassBuilder& builder) { builder.Write(buffer, RenderGraphResourceState::TransferWrite); },
                  [&](RenderGraphContext&) { executionOrder.push_back("Upload"); });
    graph.AddPass("Consume", RenderGraphQueue::Compute,
                  [&](RenderGraphPassBuilder& builder) { builder.Read(buffer, RenderGraphResourceState::ShaderRead); },
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
    graph.AddPass("ReadHistory", RenderGraphQueue::Compute,
                  [&](RenderGraphPassBuilder& builder) { builder.Read(history.Read); });
    graph.AddPass("WriteHistory", RenderGraphQueue::Compute,
                  [&](RenderGraphPassBuilder& builder) { builder.Write(history.Write); });

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
      graph.ImportTexture("Output", ColorTexture(), 7, RenderGraphResourceState::ShaderRead,
                          RenderGraphResourceState::Present);
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
    const RenderGraphPassHandle firstRead = graph.AddPass(
      "FirstWrite", RenderGraphQueue::Graphics,
      [&](RenderGraphPassBuilder& builder) { builder.Write(first, RenderGraphResourceState::ColorAttachment); });
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
      "FirstPass", RenderGraphQueue::Compute,
      [&](RenderGraphPassBuilder& builder) { builder.Write(first); },
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
      "WriteCommands", RenderGraphQueue::Compute,
      [&](RenderGraphPassBuilder& builder) { builder.Write(buffer); },
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
    const RenderGraphResourceHandle buffer = graph.ImportBuffer(
      "Instances", { 64, 16, GpuBufferType::Structured }, reinterpret_cast<uint64_t>(bufferResource.get()),
      RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
    const RenderGraphResourceHandle target = graph.ImportTexture(
      "Output", ColorTexture(), reinterpret_cast<uint64_t>(targetResource.get()),
      RenderGraphResourceState::ColorAttachment, RenderGraphResourceState::ColorAttachment);
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
