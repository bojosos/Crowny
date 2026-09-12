#include "cwepch.h"

#include "Editor/EditorLayer.h"
#include "Editor/ProjectLibrary.h"

#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"

#include <cstdio>
#include <fstream>

namespace Crowny
{
    void EditorLayer::CaptureLaunchRender()
    {
        if (m_LaunchOptions.RenderOutput.empty() || m_LaunchScenePending || m_LaunchPlayPending || ProjectLibrary::Get().IsImporting())
            return;
        if (++m_LaunchRenderFrames < m_LaunchOptions.Frames)
            return;

        const Path output = std::exchange(m_LaunchOptions.RenderOutput, {});
        try
        {
            const Ref<Texture> texture = m_RenderTarget->GetColorTexture(0);
            if (!texture || texture->GetWidth() != m_LaunchOptions.Width || texture->GetHeight() != m_LaunchOptions.Height)
                throw std::runtime_error("Could not create capture target at the requested dimensions");
            // The render thread has finished recording, but Vulkan readback only waits for submitted writes.
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            PixelData pixels(texture->GetWidth(), texture->GetHeight(), 1u, TextureFormat::RGBA8);
            pixels.AllocateInternalBuffer();
            texture->ReadData(pixels);
            if (!pixels.IsValid())
                throw std::runtime_error("Texture readback failed");

            fs::create_directories(output.parent_path());
            std::ofstream stream(output, std::ios::binary | std::ios::trunc);
            if (!stream)
                throw std::runtime_error("Could not create capture file");
            const auto writeInteger = [&](uint32_t value, uint32_t bytes) {
                for (uint32_t byte = 0; byte < bytes; ++byte)
                    stream.put(static_cast<char>((value >> (byte * 8u)) & 0xffu));
            };
            const uint32_t width = texture->GetWidth();
            const uint32_t height = texture->GetHeight();
            const uint32_t size = width * height * 4u;
            writeInteger(0x4d42u, 2);
            writeInteger(54u + size, 4);
            writeInteger(0, 4);
            writeInteger(54, 4);
            writeInteger(40, 4);
            writeInteger(width, 4);
            writeInteger(height, 4);
            writeInteger(1, 2);
            writeInteger(32, 2);
            writeInteger(0, 4);
            writeInteger(size, 4);
            writeInteger(2835, 4);
            writeInteger(2835, 4);
            writeInteger(0, 4);
            writeInteger(0, 4);

            Vector<uint8_t> row(static_cast<size_t>(width) * 4u);
            for (uint32_t y = 0; y < height; ++y)
            {
                // The editor displays both backends with vertically flipped UVs. BMP's bottom-up rows match that view.
                const uint8_t* source = pixels.GetData() + static_cast<size_t>(y) * pixels.GetRowPitch();
                for (uint32_t x = 0; x < width; ++x)
                {
                    row[x * 4u] = source[x * 4u + 2u];
                    row[x * 4u + 1u] = source[x * 4u + 1u];
                    row[x * 4u + 2u] = source[x * 4u];
                    row[x * 4u + 3u] = 255;
                }
                stream.write(reinterpret_cast<const char*>(row.data()), row.size());
            }
            stream.close();
            if (!stream)
                throw std::runtime_error("Could not finish writing capture file");
            std::printf("Rendered %s (%ux%u)\n", output.string().c_str(), width, height);
            if (m_LaunchOptions.Quit)
                Application::Get().Exit();
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Capture failed for %s: %s\n", output.string().c_str(), error.what());
            Application::Get().Exit(1);
        }
    }
} // namespace Crowny
