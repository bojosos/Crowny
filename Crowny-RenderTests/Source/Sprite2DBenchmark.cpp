#include "Sprite2DRenderTests.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Scene/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace Crowny::RenderTests
{
    int RunSprite2DBenchmark(const Path& artifacts, bool smoke)
    {
        using Clock = std::chrono::steady_clock;
        constexpr uint32_t spriteCount = 100000, width = 1920, height = 1080, seed = 0x43573244;
        const uint32_t warmup = smoke ? 5 : 300, measured = smoke ? 10 : 1800;
        const auto milliseconds = [](auto begin, auto end) { return std::chrono::duration<double, std::milli>(end - begin).count(); };
        RenderAPI& api = RenderAPI::Get();
        const auto window = Application::Get().GetRenderWindow();
        if (window->GetProperties().Width != width || window->GetProperties().Height != height)
            throw std::runtime_error("Sprite benchmark requires an exact 1920x1080 framebuffer");
        window->SetVSync(false);

        struct RendererScope
        {
            ~RendererScope() { SceneRenderer::ShutdownRenderThreadResources(); }
        } rendererScope;
        const auto scene = CreateRef<Scene>(false);
        SceneRenderer renderer(scene, window);
        Camera camera(glm::ortho(0.0f, float(width), 0.0f, float(height), -1.0f, 1.0f));
        std::array<AssetHandle<Texture>, 4> pages;
        const std::array<glm::vec4, 4> colors = { glm::vec4(1, .25f, .1f, 1), glm::vec4(.1f, 1, .25f, 1), glm::vec4(.25f, .1f, 1, 1),
                                                  glm::vec4(1, .8f, .1f, 1) };
        for (uint32_t page = 0; page < pages.size(); ++page)
        {
            TextureDesc desc;
            desc.Width = desc.Height = 256;
            desc.Format = TextureFormat::RGBA8;
            desc.sRGB = false;
            const auto texture = Texture::Create(desc);
            PixelData pixels(256, 256, 1, TextureFormat::RGBA8);
            pixels.AllocateInternalBuffer();
            for (uint32_t y = 0; y < 256; ++y)
                for (uint32_t x = 0; x < 256; ++x)
                    for (uint32_t channel = 0; channel < 4; ++channel)
                        pixels.GetData()[y * pixels.GetRowPitch() + x * 4 + channel] = uint8_t(colors[page][channel] * 255);
            texture->WriteData(pixels);
            pages[page] = static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(texture));
        }
        struct Motion
        {
            Entity Object;
            glm::vec2 Position;
            glm::vec2 Velocity;
        };
        Vector<Motion> motions;
        motions.reserve(spriteCount);
        uint32_t random = seed;
        const auto next = [&]() {
            random = random * 1664525u + 1013904223u;
            return float(random >> 8) / 16777216.0f;
        };
        for (uint32_t i = 0; i < spriteCount; ++i)
        {
            Entity entity = scene->CreateEntity("Benchmark sprite");
            auto& sprite = entity.AddComponent<SpriteRendererComponent>();
            sprite.Texture = pages[i % pages.size()];
            sprite.Color = glm::vec4(1, 1, 1, .5f);
            entity.GetTransform().SetScale({ 8, 8, 1 });
            const glm::vec2 position(4 + next() * (width - 8), 4 + next() * (height - 8));
            const glm::vec2 velocity(i & 1 ? .25f : -.25f, i & 2 ? .125f : -.125f);
            motions.push_back({ entity, position, velocity });
        }
        std::cout << "Sprite benchmark: " << spriteCount << " ECS sprites, " << width << 'x' << height << ", warm-up " << warmup << ", measured "
                  << measured << (smoke ? " (smoke only)" : "") << std::endl;

        struct Sample
        {
            double FrameMs = 0, UpdateMs = 0, ExtractMs = 0, RenderMs = 0, PresentMs = 0, GpuMs = -1;
            double UploadMs = 0, PrepareMs = 0, SubmitMs = 0;
            uint64_t UpdateAllocations = 0, ExtractAllocations = 0, RenderAllocations = 0, PresentAllocations = 0, UploadBytes = 0;
            uint32_t Visible = 0, Batches = 0;
            uint32_t DrawListCacheHits = 0;
            uint64_t VisibilityFrameNumber = 0;
            bool VisibilitySampleValid = false;
        };
        Vector<Sample> samples(warmup + measured);
        struct PendingQuery
        {
            Ref<TimerQuery> Query = TimerQuery::Create();
            uint32_t Frame = 0;
            bool Pending = false;
        };
        std::array<PendingQuery, 16> queries;
        const auto collect = [&](PendingQuery& query) {
            if (query.Pending && query.Query->IsReady())
            {
                samples[query.Frame].GpuMs = query.Query->GetTimeMs();
                query.Pending = false;
            }
        };
        RenderSnapshot snapshot;
        for (uint32_t frame = 0; frame < samples.size(); ++frame)
        {
            Sample& sample = samples[frame];
            const auto start = Clock::now();
            Window::PollEvents();
            PendingQuery& query = queries[frame % queries.size()];
            collect(query);
            const auto updateStart = Clock::now();
            const auto beforeUpdate = Memory::GetThreadAllocationSnapshot();
            for (Motion& motion : motions)
            {
                motion.Position += motion.Velocity;
                if (motion.Position.x < 4)
                    motion.Position.x += width - 8;
                if (motion.Position.x > width - 4)
                    motion.Position.x -= width - 8;
                if (motion.Position.y < 4)
                    motion.Position.y += height - 8;
                if (motion.Position.y > height - 4)
                    motion.Position.y -= height - 8;
                motion.Object.GetTransform().SetPosition({ motion.Position, 0 });
            }
            const auto afterUpdate = Memory::GetThreadAllocationSnapshot();
            const auto updated = Clock::now();
            snapshot.FrameNumber = frame + 1;
            renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1), false);
            const auto afterExtract = Memory::GetThreadAllocationSnapshot();
            const auto extracted = Clock::now();
            if (!query.Pending)
                query.Query->Begin();
            const auto beforeRender = Memory::GetThreadAllocationSnapshot();
            const auto renderStart = Clock::now();
            SceneRenderer::RenderFromSnapshot(snapshot);
            const auto rendered = Clock::now();
            const auto afterRender = Memory::GetThreadAllocationSnapshot();
            if (!query.Pending)
            {
                query.Query->End();
                query.Frame = frame;
                query.Pending = true;
            }
            api.SwapBuffers(window);
            const auto presented = Clock::now();
            const auto afterPresent = Memory::GetThreadAllocationSnapshot();
            const auto stats = SceneRenderer::GetStatistics();
            if (!stats.RenderGraphSucceeded || stats.SubmittedSprites2D != spriteCount ||
                (stats.SpriteVisibilitySampleValid && stats.VisibleSprites2D != spriteCount) ||
                (frame >= warmup && (!stats.SpriteVisibilitySampleValid || stats.SpriteVisibilityFrameNumber + 16 < snapshot.FrameNumber)))
                throw std::runtime_error("Sprite benchmark lost visible ECS sprites or fell back from the render graph");
            sample.FrameMs = milliseconds(start, presented);
            sample.UpdateMs = milliseconds(updateStart, updated);
            sample.ExtractMs = milliseconds(updated, extracted);
            sample.RenderMs = milliseconds(renderStart, rendered);
            sample.PresentMs = milliseconds(rendered, presented);
            sample.UpdateAllocations = afterUpdate.AllocationCount - beforeUpdate.AllocationCount;
            sample.ExtractAllocations = afterExtract.AllocationCount - afterUpdate.AllocationCount;
            sample.RenderAllocations = afterRender.AllocationCount - beforeRender.AllocationCount;
            sample.PresentAllocations = afterPresent.AllocationCount - afterRender.AllocationCount;
            sample.UploadBytes = stats.UploadedBytes2D;
            sample.Visible = stats.VisibleSprites2D;
            sample.VisibilityFrameNumber = stats.SpriteVisibilityFrameNumber;
            sample.VisibilitySampleValid = stats.SpriteVisibilitySampleValid;
            sample.Batches = stats.SpriteBatches2D;
            sample.DrawListCacheHits = stats.SpriteDrawListCacheHits2D;
            sample.UploadMs = stats.SpriteUploadCpuTimeMs;
            sample.PrepareMs = stats.SpritePrepareCpuTimeMs;
            sample.SubmitMs = stats.SpriteSubmissionCpuTimeMs;
            if ((frame + 1) % 300 == 0)
                std::cout << "Completed " << frame + 1 << " frames" << std::endl;
        }
        // Drain timing results after measurement. Reuse the normal presentation
        // retirement path. Visibility diagnostics are asynchronous and never drive draws.
        const auto deadline = Clock::now() + std::chrono::seconds(10);
        while (Clock::now() < deadline)
        {
            api.SwapBuffers(window);
            bool pending = false;
            for (auto& query : queries)
            {
                collect(query);
                pending |= query.Pending;
            }
            if (!pending)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::ofstream csv(artifacts / "sprites.csv");
        csv << "frame,frame_ms,update_ms,extract_ms,render_ms,present_ms,gpu_ms,update_allocations,extract_allocations,render_allocations,present_"
               "allocations,upload_"
               "bytes,visible,batches,visibility_frame,visibility_valid,draw_list_cache_hits,upload_ms,prepare_ms,submit_ms\n";
        Vector<double> times;
        times.reserve(measured);
        uint64_t renderAllocations = 0, extractAllocations = 0, presentAllocations = 0;
        uint32_t missingGpuSamples = 0;
        for (uint32_t frame = warmup; frame < samples.size(); ++frame)
        {
            const Sample& sample = samples[frame];
            csv << std::setprecision(9) << frame - warmup << ',' << sample.FrameMs << ',' << sample.UpdateMs << ',' << sample.ExtractMs << ','
                << sample.RenderMs << ',' << sample.PresentMs << ',' << sample.GpuMs << ',' << sample.UpdateAllocations << ','
                << sample.ExtractAllocations << ',' << sample.RenderAllocations << ',' << sample.PresentAllocations << ',' << sample.UploadBytes
                << ',' << sample.Visible << ',' << sample.Batches << ',' << sample.VisibilityFrameNumber << ',' << sample.VisibilitySampleValid << ','
                << sample.DrawListCacheHits << ',' << sample.UploadMs << ',' << sample.PrepareMs << ',' << sample.SubmitMs << '\n';
            times.push_back(sample.FrameMs);
            renderAllocations += sample.RenderAllocations;
            extractAllocations += sample.ExtractAllocations;
            presentAllocations += sample.PresentAllocations;
            missingGpuSamples += sample.GpuMs < 0;
        }
        std::sort(times.begin(), times.end());
        const double p95 = times[static_cast<size_t>(std::ceil(times.size() * .95)) - 1];
        std::ofstream report(artifacts / "sprites-summary.json");
        report << "{\n  \"device\": " << std::quoted(api.GetCapabilities().DeviceName)
               << ",\n  \"backend\": " << std::quoted(api.GetCapabilities().RenderAPIName) << ",\n  \"sprites\": " << spriteCount
               << ",\n  \"width\": " << width << ",\n  \"height\": " << height << ",\n  \"seed\": " << seed << ",\n  \"warmup_frames\": " << warmup
               << ",\n  \"measured_frames\": " << measured << ",\n  \"smoke\": " << (smoke ? "true" : "false")
               << ",\n  \"submission_mode\": \"single-thread snapshots\",\n  \"p95_frame_ms\": " << p95
               << ",\n  \"p95_target_met\": " << (!smoke && p95 <= 16.67 ? "true" : "false")
               << ",\n  \"renderer_cpp_allocations\": " << renderAllocations << ",\n  \"extraction_cpp_allocations\": " << extractAllocations
               << ",\n  \"presentation_cpp_allocations\": " << presentAllocations << ",\n  \"missing_gpu_samples\": " << missingGpuSamples << "\n}\n";
        std::cout << api.GetCapabilities().DeviceName << ": p95 " << p95 << " ms, render allocations " << renderAllocations
                  << ", extraction allocations " << extractAllocations << ", missing GPU samples " << missingGpuSamples << std::endl;
        if (!csv || !report)
            throw std::runtime_error("Could not write sprite benchmark results");
        // Capture the final snapshot after timing has finished. This separate
        // offscreen draw/readback is never part of a measured frame.
        TextureDesc captureDesc;
        captureDesc.Width = width;
        captureDesc.Height = height;
        captureDesc.Format = TextureFormat::RGBA8;
        captureDesc.Usage = TEXTURE_RENDERTARGET;
        captureDesc.sRGB = false;
        const auto captureColor = Texture::Create(captureDesc);
        RenderTextureDesc targetDesc;
        targetDesc.Width = width;
        targetDesc.Height = height;
        targetDesc.ColorSurfaces[0].Texture = captureColor;
        snapshot.Target = RenderTexture::Create(targetDesc);
        SceneRenderer::RenderFromSnapshot(snapshot);
        api.SubmitCommandBuffer(nullptr);
        PixelData pixels(width, height, 1, TextureFormat::RGBA8);
        pixels.AllocateInternalBuffer();
        captureColor->ReadData(pixels);
        Image capture(width, height);
        uint64_t covered = 0;
        for (uint32_t y = 0; y < height; ++y)
        {
            std::memcpy(capture.Pixel(0, y), pixels.GetData() + y * pixels.GetRowPitch(), width * 4);
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint8_t* pixel = capture.Pixel(x, y);
                covered += pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0;
            }
        }
        String captureError;
        if (!SaveBmp(artifacts / "sprites-final.bmp", capture, captureError))
            throw std::runtime_error(captureError);
        // The seeded workload covers about 95% of the output. A missing storage
        // or order page falls below this conservative check.
        if (covered < uint64_t(width) * height * 9 / 10)
            throw std::runtime_error("Sprite benchmark final capture has insufficient rendered coverage");
        // This diagnostic reports the fixed workload; passing timings alone do
        // not certify the roadmap's player, allocation, and backend gates.
        return 0;
    }
} // namespace Crowny::RenderTests
