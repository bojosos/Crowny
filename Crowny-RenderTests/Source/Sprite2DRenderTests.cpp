#include "Sprite2DRenderTests.h"

#include "Crowny/Assets/AssetManager.h"

#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/GpuWorld2D.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Utils/PixelUtils.h"

namespace Crowny::RenderTests
{
    bool RenderIntegerClearDraw(Image& image, String& error)
    {
        Renderer2D::Init();
        struct RendererScope
        {
            ~RendererScope() { Renderer2D::Shutdown(); }
        } rendererScope;
        RenderAPI& api = RenderAPI::Get();
        // Odd dimensions and one-pixel targets exercise both split directions
        // and the unsplittable case of the Intel integer-clear workaround.
        for (const auto size : { glm::uvec2(65, 33), glm::uvec2(1, 65), glm::uvec2(65, 1), glm::uvec2(1, 1) })
        {
            TextureDesc desc;
            desc.Width = size.x;
            desc.Height = size.y;
            desc.Format = TextureFormat::RGBA8;
            desc.Usage = TEXTURE_RENDERTARGET;
            desc.sRGB = false;
            const auto color = Texture::Create(desc);
            desc.Format = TextureFormat::R32I;
            const auto ids = Texture::Create(desc);
            RenderTextureDesc targetDesc;
            targetDesc.Width = size.x;
            targetDesc.Height = size.y;
            targetDesc.ColorSurfaces[0].Texture = color;
            targetDesc.ColorSurfaces[1].Texture = ids;
            const auto target = RenderTexture::Create(targetDesc);
            PixelData values(size.x, size.y, 1, TextureFormat::R32I);
            values.AllocateInternalBuffer();
            const glm::vec4 background(0.02f, 0.03f, 0.04f, 1);
            for (uint32_t frame = 0; frame < 4; ++frame)
            {
                api.SetRenderTarget(target);
                api.SetViewport(0, 0, 1, 1);
                api.ClearRenderTarget(FBT_COLOR, background);
                api.SubmitCommandBuffer(nullptr);
                int32_t initial = 0;
                if (!ids->ReadPixel(0, 0, &initial, sizeof(initial)) || initial != glm::floatBitsToInt(background.x))
                {
                    error = "Integer attachment clear did not preserve the requested bits";
                    return false;
                }
                // Submission starts a fresh Vulkan command buffer.
                api.SetRenderTarget(target);
                api.SetViewport(0, 0, 1, 1);
                api.ClearRenderTarget(FBT_COLOR, background, 1, 0, 0x01);
                api.ClearRenderTarget(FBT_COLOR, glm::vec4(0), 1, 0, 0x02);
                Renderer2D::Begin(glm::mat4(1), glm::mat4(1));
                glm::mat4 transform = glm::scale(glm::mat4(1), glm::vec3(0.25f));
                transform[3].z = 0.5f;
                const bool transparent = frame == 2;
                Renderer2D::FillRect(transform, nullptr, glm::vec4(1, 1, 1, transparent ? 0 : 1), 41);
                Renderer2D::End();
                api.SubmitCommandBuffer(nullptr);
                ids->ReadData(values);
                for (uint32_t y = 0; y < size.y; ++y)
                    for (uint32_t x = 0; x < size.x; ++x)
                    {
                        int32_t value;
                        std::memcpy(&value, values.GetData() + y * values.GetRowPitch() + x * 4, 4);
                        const int32_t centerId = transparent ? 0 : 41;
                        if ((value != 0 && value != centerId) || (x == size.x / 2 && y == size.y / 2 && value != centerId))
                        {
                            error = "Clear followed by a partial draw left stale integer pixels at " + std::to_string(size.x) + "x" +
                                    std::to_string(size.y) + ", frame " + std::to_string(frame) + ", pixel " + std::to_string(x) + "," +
                                    std::to_string(y) + ": " + std::to_string(value);
                            return false;
                        }
                    }
            }
            if (size.x == 65 && size.y == 33)
            {
                PixelData pixels(size.x, size.y, 1, TextureFormat::RGBA8);
                pixels.AllocateInternalBuffer();
                color->ReadData(pixels);
                image = Image(size.x, size.y);
                for (uint32_t y = 0; y < size.y; ++y)
                    std::memcpy(image.Pixel(0, y), pixels.GetData() + y * pixels.GetRowPitch(), size.x * 4);
            }
        }
        return true;
    }

    bool RenderPersistentText(Image& image, String& error)
    {
        constexpr uint32_t width = 256, height = 128;
        TextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = TextureFormat::RGBA8;
        desc.Usage = TEXTURE_RENDERTARGET;
        desc.sRGB = false;
        const auto color = Texture::Create(desc);
        desc.Format = TextureFormat::R32I;
        const auto ids = Texture::Create(desc);
        RenderTextureDesc targetDesc;
        targetDesc.Width = width;
        targetDesc.Height = height;
        targetDesc.ColorSurfaces[0].Texture = color;
        targetDesc.ColorSurfaces[1].Texture = ids;
        const auto target = RenderTexture::Create(targetDesc);
        const auto font = AssetManager::Get().Load<Font>("Resources/Fonts/Roboto/roboto-thin.ttf.asset");
        if (!target || !font)
        {
            error = "Persistent text test requires render targets and the built-in font";
            return false;
        }
        RenderableText text;
        text.TextData.Font = font;
        text.TextData.Text = "Persistent text\nMoves without uploads";
        text.TextData.Size = 36;
        text.TextData.Color = { 0.8f, 0.9f, 1, 0.85f };
        text.TextData.OutlineColor = { 0.1f, 0.4f, 0.8f, 1 };
        text.TextData.Thickness = 0.7f;
        text.TextData.ShadowColor = { 0.6f, 0.1f, 0.2f, 0.8f };
        text.TextData.ShadowOffset = { 0.07f, -0.06f };
        text.TextData.ShadowSoftness = 0.3f;
        text.WorldMatrix = glm::mat4(1);
        text.WorldMatrix[0].x = 0.19f;
        text.WorldMatrix[1].y = 0.35f;
        // Avoid exact pixel-center ties on glyph rectangle edges, where tiny
        // backend coordinate differences can change boundary sample coverage.
        text.WorldMatrix[3] = { -0.9f, 0.5f + 0.5f / height, 0.5f, 1 };
        text.EntityId = 41;
        TextLayoutCache cache;
        cache.BeginExtraction();
        text.Layout = cache.Get(1, text.TextData);
        cache.EndExtraction();
        if (!text.Layout || text.Layout->View().GlyphCount < 8)
        {
            error = "Could not lay out persistent text fixture";
            return false;
        }
        GpuText2D gpu(4); // Force page splits within each shadow and foreground run.
        RenderAPI& api = RenderAPI::Get();
        const glm::vec4 background(0.02f, 0.03f, 0.04f, 1);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, background);
        api.SubmitCommandBuffer(nullptr);
        int32_t clearedId = 0;
        if (!ids->ReadPixel(0, 0, &clearedId, sizeof(clearedId)) || clearedId != glm::floatBitsToInt(background.x))
        {
            error = "Mixed float/integer attachments did not preserve the clear value bits";
            return false;
        }
        // This fixture renders directly to backend targets, without the scene's
        // presentation pass. Keep positive world Y at the top of the capture.
        glm::mat4 projection(1);
        if (RenderAPI::GetAPI() == RenderAPI::API::Vulkan)
            projection[1][1] = -1;
        auto begin = [&]() {
            api.SetRenderTarget(target);
            api.SetViewport(0, 0, 1, 1);
            api.ClearRenderTarget(FBT_COLOR, background, 1, 0, 0x01);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0), 1, 0, 0x02);
        };
        auto draw = [&]() {
            begin();
            gpu.BeginView();
            return gpu.Prepare(text.Layout) && gpu.Render(text, projection);
        };
        if (!draw() || gpu.GetStatistics().GeometryBuilds != 1 || !gpu.GetStatistics().GeometryUploadBytes || gpu.GetStatistics().Batches < 4)
        {
            error = "Persistent text did not create bounded glyph pages";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        text.WorldMatrix[3].x += 0.05f;
        text.TextData.Color.a = 0.7f;
        text.TextData.FontStyle.Set(TextFontStyleBits::Italic);
        text.TextData.FontStyle.Set(TextFontStyleBits::Underline);
        text.TextData.FontStyle.Set(TextFontStyleBits::Strikethrough);
        text.TextData.DecorationThickness = 0.035f;
        text.TextData.ClipToBounds = true;
        text.TextData.LayoutSize = { 8, 3 };
        if (!draw() || gpu.GetStatistics().GeometryUploadBytes || gpu.GetStatistics().GeometryBuilds)
        {
            error = "Moving or repainting text rebuilt or uploaded glyph geometry";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        PixelData retained(width, height, 1, TextureFormat::RGBA8);
        retained.AllocateInternalBuffer();
        color->ReadData(retained);
        PixelData retainedIds(width, height, 1, TextureFormat::R32I);
        retainedIds.AllocateInternalBuffer();
        ids->ReadData(retainedIds);
        // Compare with the compatibility renderer, including effects and picking.
        Renderer2D::Init();
        begin();
        Renderer2D::Begin(projection, glm::mat4(1));
        Renderer2D::DrawString(text.TextData, text.WorldMatrix, text.EntityId, text.Layout.get());
        Renderer2D::End();
        api.SubmitCommandBuffer(nullptr);
        PixelData legacy(width, height, 1, TextureFormat::RGBA8);
        legacy.AllocateInternalBuffer();
        color->ReadData(legacy);
        PixelData legacyIds(width, height, 1, TextureFormat::R32I);
        legacyIds.AllocateInternalBuffer();
        ids->ReadData(legacyIds);
        Renderer2D::Shutdown();
        uint32_t colorDifferences = 0, idDifferences = 0, pickedPixels = 0;
        uint32_t invalidRetainedIds = 0, invalidLegacyIds = 0;
        image = Image(width, height);
        for (uint32_t y = 0; y < height; ++y)
        {
            const uint32_t sourceY = RenderAPI::GetAPI() == RenderAPI::API::OpenGL ? height - y - 1 : y;
            std::memcpy(image.Pixel(0, y), retained.GetData() + sourceY * retained.GetRowPitch(), width * 4);
            for (uint32_t x = 0; x < width; ++x)
            {
                for (uint32_t c = 0; c < 4; ++c)
                    colorDifferences += std::abs(int(retained.GetData()[y * retained.GetRowPitch() + x * 4 + c]) -
                                                 int(legacy.GetData()[y * legacy.GetRowPitch() + x * 4 + c])) > 3;
                int32_t retainedId, legacyId;
                std::memcpy(&retainedId, retainedIds.GetData() + y * retainedIds.GetRowPitch() + x * 4, 4);
                std::memcpy(&legacyId, legacyIds.GetData() + y * legacyIds.GetRowPitch() + x * 4, 4);
                idDifferences += retainedId != legacyId;
                invalidRetainedIds += retainedId != 0 && retainedId != text.EntityId;
                invalidLegacyIds += legacyId != 0 && legacyId != text.EntityId;
                pickedPixels += retainedId == text.EntityId;
            }
        }
        if (colorDifferences + idDifferences > 32 || pickedPixels < 100 || invalidRetainedIds || invalidLegacyIds)
        {
            error = "Retained text differs from compatibility rendering: " + std::to_string(colorDifferences) + " color samples, " +
                    std::to_string(idDifferences) + " IDs; picked pixels: " + std::to_string(pickedPixels) +
                    "; invalid IDs (retained/legacy): " + std::to_string(invalidRetainedIds) + "/" + std::to_string(invalidLegacyIds);
            return false;
        }
        const auto visibleWorld = text.WorldMatrix;
        const auto visiblePaint = text.TextData;
        text.WorldMatrix[3].x = 100;
        if (!draw() || gpu.GetStatistics().CulledObjects != 1 || gpu.GetStatistics().Batches || gpu.GetStatistics().UniformUploadBytes ||
            gpu.GetStatistics().GeometryUploadBytes)
        {
            error = "Offscreen retained text submitted work or uploaded geometry";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        text.TextData.ClipToBounds = false;
        text.TextData.ShadowOffset.x += (visibleWorld[3].x - text.WorldMatrix[3].x) / text.WorldMatrix[0].x;
        if (!draw() || gpu.GetStatistics().CulledObjects || !gpu.GetStatistics().Batches)
        {
            error = "Text culling ignored a shadow that reaches the viewport";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        text.WorldMatrix = visibleWorld;
        text.TextData = visiblePaint;
        text.WorldMatrix[3].y = 100;
        text.TextData.ClipToBounds = false;
        text.TextData.ShadowColor.a = 0;
        text.TextData.UnderlineOffset += (visibleWorld[3].y - text.WorldMatrix[3].y) / text.WorldMatrix[1].y;
        if (!draw() || gpu.GetStatistics().CulledObjects || !gpu.GetStatistics().Batches)
        {
            error = "Text culling ignored a decoration that reaches the viewport";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        text.WorldMatrix = visibleWorld;
        text.TextData = visiblePaint;
        // A second view shares the cache. Removing the extraction and snapshot
        // references retires it without a camera-specific lifetime dependency.
        begin();
        gpu.BeginView();
        glm::mat4 secondCamera = projection;
        secondCamera[3].x = -0.1f;
        if (!gpu.Prepare(text.Layout) || !gpu.Render(text, secondCamera) || gpu.GetStatistics().GeometryUploadBytes)
        {
            error = "A second text camera invalidated shared glyph pages";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        {
            RenderSnapshot snapshot;
            snapshot.ProjectionMatrix = projection;
            snapshot.ViewMatrix = glm::mat4(1);
            snapshot.Texts.Acquire() = text;
            auto& order = snapshot.Ordered2D.Acquire();
            order = {};
            order.Type = Renderable2DType::Text;
            GpuWorld2D world(65536, 1); // Sprite/text switches cross order pages.
            begin();
            if (!world.Render(snapshot) || world.GetStatistics().TextGeometryBuilds != 1 || !world.GetStatistics().GlyphUploadBytes ||
                world.GetStatistics().OrderUploadBytes)
            {
                error = "Scene text did not use the retained glyph path";
                return false;
            }
            api.SubmitCommandBuffer(nullptr);
            snapshot.HistoryNamespace = 2;
            snapshot.Texts[0].WorldMatrix[3].x += 0.1f;
            begin();
            if (!world.Render(snapshot) || world.GetStatistics().TextGeometryBuilds || world.GetStatistics().GlyphUploadBytes)
            {
                error = "Scene text movement or camera changes reuploaded glyph geometry";
                return false;
            }
            api.SubmitCommandBuffer(nullptr);
            RenderWorld2D sprites;
            for (uint32_t i = 0; i < 2; ++i)
            {
                RenderInstance2DDesc spriteDesc;
                spriteDesc.Transform[0].x = i ? 1.0f : 2.0f;
                spriteDesc.Transform[1].y = 2;
                spriteDesc.Transform[3] = { i ? 0.5f : 0.0f, 0, 0.5f, 1 };
                spriteDesc.Color = i ? glm::vec4(0, 0, 1, 0.5f) : glm::vec4(1, 0, 0, 0.25f);
                spriteDesc.ObjectID = { 78 + i };
                auto& sprite = snapshot.Sprites.Acquire();
                sprite.Handle = sprites.Create(spriteDesc);
                sprite.WorldMatrix = spriteDesc.Transform;
            }
            Vector<RenderChange2D> changes;
            sprites.DrainChanges(changes);
            world.Apply(changes);
            snapshot.Texts[0].WorldMatrix = text.WorldMatrix;
            snapshot.Ordered2D.Reset();
            for (uint32_t i = 0; i < 3; ++i)
            {
                auto& item = snapshot.Ordered2D.Acquire();
                item = {};
                item.Type = i == 1 ? Renderable2DType::Text : Renderable2DType::Sprite;
                item.Index = i == 2 ? 1 : 0;
            }
            begin();
            if (!world.Render(snapshot) || world.GetStatistics().GlyphUploadBytes)
            {
                error = "Mixed scene submission lost retained glyph pages";
                return false;
            }
            api.SubmitCommandBuffer(nullptr);
            PixelData mixedIds(width, height, 1, TextureFormat::R32I);
            mixedIds.AllocateInternalBuffer();
            ids->ReadData(mixedIds);
            uint32_t checked = 0;
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    int32_t textId, mixedId;
                    std::memcpy(&textId, retainedIds.GetData() + y * retainedIds.GetRowPitch() + x * 4, 4);
                    if (textId != text.EntityId)
                        continue;
                    std::memcpy(&mixedId, mixedIds.GetData() + y * mixedIds.GetRowPitch() + x * 4, 4);
                    if (mixedId != (x < width / 2 ? text.EntityId : 79))
                    {
                        error = "Sprite/text/sprite order changed across retained glyph pages";
                        return false;
                    }
                    ++checked;
                }
            if (checked < 100)
                return false;

            // Exercise the actual scene graph, including MRT picking, rather
            // than only the retained renderer's direct-target adapter.
            struct SceneResourcesScope
            {
                ~SceneResourcesScope() { SceneRenderer::ShutdownRenderThreadResources(); }
            } sceneResources;
            TextureDesc depthDesc;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.Format = TextureFormat::DEPTH32F;
            depthDesc.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            const auto sceneDepth = Texture::Create(depthDesc);
            RenderTextureDesc sceneTargetDesc = targetDesc;
            sceneTargetDesc.DepthSurface.Texture = sceneDepth;
            snapshot.Target = RenderTexture::Create(sceneTargetDesc);
            snapshot.EnableObjectID = true;
            snapshot.FrameNumber = 1;
            snapshot.HistoryOwnerId = 90001;
            snapshot.HistoryNamespace = 90002;
            snapshot.World2DLifetime = std::make_shared<const uint8_t>(0);
            for (const auto& change : changes)
                snapshot.RenderWorld2DChanges.Acquire() = change;
            SceneRenderer::RenderFromSnapshot(snapshot);
            api.SubmitCommandBuffer(nullptr);
            std::array<uint8_t, 4> leftColor{}, rightColor{};
            if (!color->ReadPixel(2, 2, leftColor.data(), leftColor.size()) ||
                !color->ReadPixel(width - 3, 2, rightColor.data(), rightColor.size()) || std::abs(int(leftColor[0]) - 64) > 1 || leftColor[1] ||
                leftColor[2] || leftColor[3] != 255 || std::abs(int(rightColor[0]) - 32) > 1 || rightColor[1] ||
                std::abs(int(rightColor[2]) - 128) > 1 || rightColor[3] != 255)
            {
                error = "2D scene graph changed premultiplied sprite compositing";
                return false;
            }
            float clearedDepth = -1;
            const float expectedDepth = api.GetCapabilities().GetFeatureTier() == RenderFeatureTier::Compatibility ? 1.0f : 0.0f;
            if (!sceneDepth->ReadPixel(0, 0, &clearedDepth, sizeof(clearedDepth)) || clearedDepth != expectedDepth)
            {
                error = "2D-only scene did not clear depth using the backend's depth convention";
                return false;
            }
            const auto statistics = SceneRenderer::GetStatistics();
            if (!statistics.RenderGraphSucceeded || statistics.GraphicsPasses != 2 || statistics.ComputePasses || statistics.TransferPasses ||
                statistics.SubmittedSprites2D != 2)
            {
                error = "2D-only scene did not use its two-pass render graph";
                return false;
            }
            PixelData sceneIds(width, height, 1, TextureFormat::R32I);
            sceneIds.AllocateInternalBuffer();
            ids->ReadData(sceneIds);
            for (uint32_t y = 0; y < height; ++y)
                if (std::memcmp(sceneIds.GetData() + y * sceneIds.GetRowPitch(), mixedIds.GetData() + y * mixedIds.GetRowPitch(), width * 4) != 0)
                {
                    error = "2D scene graph changed ordered sprite/text picking";
                    return false;
                }
        }
        cache.Clear();
        text.Layout.Reset();
        gpu.BeginView();
        if (gpu.GetCachedLayoutCount() != 0)
        {
            error = "Unreferenced text geometry was not retired";
            return false;
        }
        return true;
    }

    bool RenderMixed2DOrder(Image& image, String& error)
    {
        struct RendererScope
        {
            RendererScope() { Renderer2D::Init(); }
            ~RendererScope() { Renderer2D::Shutdown(); }
        } rendererScope;
        constexpr uint32_t size = 64;
        TextureDesc desc;
        desc.Width = desc.Height = size;
        desc.Usage = TEXTURE_RENDERTARGET;
        desc.Format = TextureFormat::RGBA8;
        desc.sRGB = false;
        const auto color = Texture::Create(desc);
        desc.Format = TextureFormat::R32I;
        const auto ids = Texture::Create(desc);
        RenderTextureDesc targetDesc;
        targetDesc.Width = targetDesc.Height = size;
        targetDesc.ColorSurfaces[0].Texture = color;
        targetDesc.ColorSurfaces[1].Texture = ids;
        const auto target = RenderTexture::Create(targetDesc);
        if (!color || !ids || !target)
        {
            error = "Could not allocate mixed 2D targets";
            return false;
        }
        RenderAPI& api = RenderAPI::Get();
        api.SetRenderTarget(target);
        api.SetViewport(0, 0, 1, 1);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        glm::mat4 transform(1);
        transform[3].z = 0.5f;
        Renderer2D::Begin(glm::mat4(1), glm::mat4(1));
        Renderer2D::FillRect(transform, nullptr, { 1, 0, 0, 0.5f }, 1);
        Renderer2D::DrawCircle(transform, { 0, 1, 0, 0.5f }, 1, 0.005f, 2);
        Renderer2D::FillRect(transform, nullptr, { 0, 0, 1, 0.5f }, 3);
        Renderer2D::End();
        api.SubmitCommandBuffer(nullptr);

        PixelData result(size, size, 1, TextureFormat::RGBA8);
        result.AllocateInternalBuffer();
        color->ReadData(result);
        image = Image(size, size);
        for (uint32_t y = 0; y < size; ++y)
        {
            const uint32_t sourceY = RenderAPI::GetAPI() == RenderAPI::API::OpenGL ? size - y - 1 : y;
            std::memcpy(image.Pixel(0, y), result.GetData() + sourceY * result.GetRowPitch(), size * 4);
        }
        const auto pixel = image.Pixel(size / 2, size / 2);
        if (std::abs(int(pixel[0]) - 32) > 1 || std::abs(int(pixel[1]) - 64) > 1 || std::abs(int(pixel[2]) - 128) > 1 || pixel[3] != 255)
        {
            error = "Mixed sprite/circle order or premultiplied blending is incorrect";
            return false;
        }
        int32_t picked = 0;
        if (!ids->ReadPixel(size / 2, size / 2, &picked, sizeof(picked)) || picked != 3)
        {
            error = "Picking did not retain the last mixed 2D item: " + std::to_string(picked);
            return false;
        }
        return true;
    }

    bool RenderPersistentSprites(Image& image, String& error)
    {
        constexpr uint32_t size = 64;
        TextureDesc textureDesc;
        textureDesc.Width = textureDesc.Height = size;
        textureDesc.Format = TextureFormat::RGBA8;
        textureDesc.Usage = TEXTURE_RENDERTARGET;
        textureDesc.sRGB = false;
        const Ref<Texture> color = Texture::Create(textureDesc);
        textureDesc.Format = TextureFormat::R32I;
        const Ref<Texture> ids = Texture::Create(textureDesc);
        RenderTextureDesc targetDesc;
        targetDesc.Width = targetDesc.Height = size;
        targetDesc.ColorSurfaces[0].Texture = color;
        targetDesc.ColorSurfaces[1].Texture = ids;
        const Ref<RenderTexture> target = RenderTexture::Create(targetDesc);
        if (!color || !ids || !target)
        {
            error = "Could not allocate sprite rendering targets";
            return false;
        }

        RenderWorld2D world;
        GpuWorld2D gpu;
        Vector<RenderChange2D> changes;
        RenderSnapshot snapshot;
        snapshot.ProjectionMatrix = glm::mat4(1);
        snapshot.ViewMatrix = glm::mat4(1);
        snapshot.Target = target;
        // Ten distinct textures force a portable texture-table boundary.
        for (uint32_t i = 0; i < 10; ++i)
        {
            TextureDesc sourceDesc;
            sourceDesc.Width = sourceDesc.Height = 1;
            sourceDesc.sRGB = false;
            const Ref<Texture> source = Texture::Create(sourceDesc);
            PixelData pixels(1, 1, 1, TextureFormat::RGBA8);
            pixels.AllocateInternalBuffer();
            pixels.GetData()[0] = static_cast<uint8_t>(20 + i * 20);
            pixels.GetData()[1] = 100;
            pixels.GetData()[2] = 200;
            pixels.GetData()[3] = 255;
            source->WriteData(pixels);
            RenderInstance2DDesc desc;
            desc.Transform[0].x = 0.16f;
            desc.Transform[1].y = 1.5f;
            desc.Transform[3].x = -0.9f + i * 0.2f;
            desc.Transform[3].z = 0.5f;
            desc.ObjectID = { i + 1 };
            desc.TextureResource = source;
            auto& sprite = snapshot.Sprites.Acquire();
            sprite.Handle = world.Create(desc);
            sprite.WorldMatrix = desc.Transform;
            sprite.Color = desc.Color;
            sprite.Texture = source;
            sprite.EntityId = static_cast<int32_t>(i + 1);
            auto& order = snapshot.Ordered2D.Acquire();
            order.Type = Renderable2DType::Sprite;
            order.Index = i;
            order.SortingLayer = 0;
            order.OrderInLayer = 0;
            order.StableOrder = i;
        }
        world.DrainChanges(changes);
        gpu.Apply(changes);
        RenderAPI& api = RenderAPI::Get();
        auto draw = [&]() {
            api.SetRenderTarget(target);
            api.SetViewport(0, 0, 1, 1);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
            return gpu.Render(snapshot);
        };
        if (!draw() || gpu.GetStatistics().Batches != 2 || gpu.GetStatistics().Visible != 10)
        {
            error = "Persistent sprite rendering failed or lost the texture-table split";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        gpu.Apply({});
        if (!draw() || gpu.GetStatistics().InstanceUploadBytes != 0 || gpu.GetStatistics().OrderUploadBytes != 0)
        {
            error = "Unchanged sprite rendering reuploaded persistent data";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        // A second camera gets its own compact order while retaining the shared
        // instance buffer. Returning to the first view must hit its order cache.
        snapshot.HistoryNamespace = 1;
        if (!draw() || gpu.GetStatistics().InstanceUploadBytes != 0 || gpu.GetStatistics().OrderUploadBytes != 10 * sizeof(DrawInstance2D))
        {
            error = "A second view reuploaded shared instance content";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        snapshot.HistoryNamespace = 0;
        if (!draw() || gpu.GetStatistics().InstanceUploadBytes != 0 || gpu.GetStatistics().OrderUploadBytes != 0)
        {
            error = "Alternating cameras invalidated a retained view order";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        gpu.ReleaseView(1);
        RenderInstance2DDesc reordered;
        const auto& firstSprite = snapshot.Sprites[0];
        reordered.Transform = firstSprite.WorldMatrix;
        reordered.Color = firstSprite.Color;
        reordered.TextureResource = firstSprite.Texture;
        reordered.ObjectID = { 1 };
        reordered.SortingLayer = 3;
        world.Update(firstSprite.Handle, reordered);
        Vector<RenderChange2D> orderingChanges;
        world.DrainChanges(orderingChanges);
        gpu.Apply(orderingChanges);
        if (!draw() || gpu.GetStatistics().InstanceUploadBytes != 0)
        {
            error = "A sorting-only change reuploaded unchanged sprite instance content";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        // Different storage/order page sizes force draws to cross order pages
        // within one storage page, including partially filled final pages.
        GpuWorld2D paged(4, 3);
        paged.Apply(changes);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        if (!paged.Render(snapshot) || paged.GetStatistics().Batches != 6 || paged.GetStatistics().Visible != 10 ||
            paged.GetStatistics().BatchBreaks[static_cast<size_t>(BatchBreak2D::OrderPage)] != 3)
        {
            error = "Persistent sprite storage pages lost an instance or failed to split draws";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        if (!paged.Render(snapshot) || paged.GetStatistics().InstanceUploadBytes != 0 || paged.GetStatistics().OrderUploadBytes != 0)
        {
            error = "Unchanged paged sprite content was uploaded again";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        // Hide complete order pages, then restore them. Their cached GPU
        // contents must survive both partial and empty views.
        const Vector<Renderable2DOrder> fullOrder(snapshot.Ordered2D.begin(), snapshot.Ordered2D.end());
        for (const uint32_t visible : { 6u, 0u, 10u })
        {
            snapshot.Ordered2D.Reset();
            for (uint32_t i = 0; i < visible; ++i)
                snapshot.Ordered2D.Acquire() = fullOrder[i];
            api.SetRenderTarget(target);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
            if (!paged.Render(snapshot) || paged.GetStatistics().Visible != visible || paged.GetStatistics().InstanceUploadBytes ||
                paged.GetStatistics().OrderUploadBytes)
            {
                error = "Temporarily unused draw-order pages lost their cached contents";
                return false;
            }
            api.SubmitCommandBuffer(nullptr);
        }
        // Change only the first order page. Storage-page and texture-table
        // boundaries still apply, but untouched order pages must not upload.
        std::swap(snapshot.Ordered2D[0], snapshot.Ordered2D[1]);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        if (!paged.Render(snapshot) || paged.GetStatistics().InstanceUploadBytes != 0 ||
            paged.GetStatistics().OrderUploadBytes != 3 * sizeof(DrawInstance2D))
        {
            error = "Changing one draw-order page uploaded unchanged pages";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        // Fill the last page and grow a new one with culled objects. Existing
        // pages and the unchanged visible order must remain resident.
        for (uint32_t i = 0; i < 3; ++i)
        {
            RenderInstance2DDesc outside;
            outside.Transform[3] = { 10, 0, 0.5f, 1 };
            auto& sprite = snapshot.Sprites.Acquire();
            sprite.Handle = world.Create(outside);
            sprite.WorldMatrix = outside.Transform;
            auto& order = snapshot.Ordered2D.Acquire();
            order = {};
            order.Index = static_cast<uint32_t>(snapshot.Sprites.Size() - 1);
        }
        Vector<RenderChange2D> growthChanges;
        world.DrainChanges(growthChanges);
        paged.Apply(growthChanges);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        constexpr uint64_t newRecordBytes = 3 * (sizeof(RenderInstance2D) + sizeof(glm::uvec4));
        if (!paged.Render(snapshot) || paged.GetStatistics().Submitted != 13 || paged.GetStatistics().Visible != 10 ||
            paged.GetStatistics().InstanceUploadBytes != newRecordBytes || paged.GetStatistics().OrderUploadBytes != 0)
        {
            error = "Growing sprite storage reuploaded existing pages or invalidated unchanged visible order";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        PixelData result(size, size, 1, TextureFormat::RGBA8);
        result.AllocateInternalBuffer();
        color->ReadData(result);
        image = Image(size, size);
        for (uint32_t y = 0; y < size; ++y)
        {
            const uint32_t sourceY = RenderAPI::GetAPI() == RenderAPI::API::OpenGL ? size - y - 1 : y;
            std::memcpy(image.Pixel(0, y), result.GetData() + sourceY * result.GetRowPitch(), size * 4);
        }
        for (uint32_t i = 0; i < 10; ++i)
        {
            const uint32_t x = static_cast<uint32_t>((0.05f + i * 0.1f) * size);
            const uint8_t* pixel = image.Pixel(x, size / 2);
            if (std::abs(int(pixel[0]) - int(20 + i * 20)) > 1 || pixel[1] != 100 || pixel[2] != 200)
            {
                error = "Sprite instance or texture selection produced the wrong stripe color at " + std::to_string(i);
                return false;
            }
            int32_t picked = 0;
            const bool read = ids->ReadPixel(x, size / 2, &picked, sizeof(picked));
            if (!read || picked != static_cast<int32_t>(i + 1))
            {
                error = "Persistent sprite object ID at stripe " + std::to_string(i) + " was " + std::to_string(picked) + ", expected " +
                        std::to_string(i + 1) + (read ? "" : " (read failed)");
                return false;
            }
        }
        // Overlap the same sprites with transparency. Page splitting must keep
        // the ordered composition and the final visible object's picking ID.
        for (uint32_t i = 0; i < 10; ++i)
        {
            RenderInstance2DDesc overlap;
            overlap.Transform[3].z = 0.5f;
            overlap.Color = i % 2 ? glm::vec4(0, 0, 1, 0.5f) : glm::vec4(1, 0, 0, 0.5f);
            overlap.ObjectID = { i + 1 };
            auto& sprite = snapshot.Sprites[i];
            world.Update(sprite.Handle, overlap);
            sprite.WorldMatrix = overlap.Transform;
            sprite.Color = overlap.Color;
        }
        world.DrainChanges(changes);
        paged.Apply(changes);
        api.SetRenderTarget(target);
        api.ClearRenderTarget(FBT_COLOR, glm::vec4(0, 0, 0, 1));
        if (!paged.Render(snapshot))
        {
            error = "Transparent sprites failed across order pages";
            return false;
        }
        api.SubmitCommandBuffer(nullptr);
        glm::vec3 expected(0);
        for (const auto& order : snapshot.Ordered2D)
            if (order.Index < 10)
                expected = glm::vec3(snapshot.Sprites[order.Index].Color) * 0.5f + expected * 0.5f;
        uint8_t blended[4]{};
        int32_t picked = 0;
        if (!color->ReadPixel(size / 2, size / 2, blended, sizeof(blended)) || !ids->ReadPixel(size / 2, size / 2, &picked, sizeof(picked)) ||
            picked != 10)
        {
            error = "Transparent order-page draws lost the last object's picking ID";
            return false;
        }
        for (uint32_t channel = 0; channel < 3; ++channel)
            if (std::abs(int(blended[channel]) - int(std::round(expected[channel] * 255))) > 2)
            {
                error = "Order-page boundaries changed transparent sprite composition";
                return false;
            }
        return true;
    }
} // namespace Crowny::RenderTests
