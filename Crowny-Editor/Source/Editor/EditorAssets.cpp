#include "cwepch.h"

#include "Editor/EditorAssets.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    namespace
    {
        Ref<Texture> CreateSettingsIcon()
        {
            constexpr uint32_t iconSize = 16u;
            TextureDesc description;
            description.Width = iconSize;
            description.Height = iconSize;
            description.Format = TextureFormat::RGBA8;
            description.GenerateMipmaps = false;
            description.sRGB = false;
            description.DebugName = "Editor/ViewportSettingsIcon";

            Ref<PixelData> pixels = PixelData::Create(iconSize, iconSize, 1u, TextureFormat::RGBA8);
            const glm::vec4 transparent(0.0f);
            const glm::vec4 white(1.0f);
            for (uint32_t y = 0u; y < iconSize; ++y)
            {
                for (uint32_t x = 0u; x < iconSize; ++x)
                    pixels->SetColorAt(x, y, transparent);
            }

            const auto drawSlider = [&](uint32_t y, uint32_t knobX) {
                for (uint32_t x = 2u; x <= 13u; ++x)
                    pixels->SetColorAt(x, y, white);
                for (uint32_t knobY = y - 1u; knobY <= y + 1u; ++knobY)
                {
                    for (uint32_t knobPixelX = knobX - 1u; knobPixelX <= knobX + 1u; ++knobPixelX)
                        pixels->SetColorAt(knobPixelX, knobY, white);
                }
            };
            drawSlider(3u, 5u);
            drawSlider(8u, 11u);
            drawSlider(13u, 7u);

            Ref<Texture> texture = Texture::Create(description);
            texture->WriteData(*pixels);
            return texture;
        }
    } // namespace

    const String EditorAssets::UnassignedTexture = "Resources/Textures/Unassigned.asset";

    const String EditorAssets::PlayIcon = "Resources/Icons/Play.asset";
    const String EditorAssets::PauseIcon = "Resources/Icons/Pause.asset";
    const String EditorAssets::StopIcon = "Resources/Icons/Stop.asset";

    const String EditorAssets::FileIcon = "Resources/Icons/File.asset";
    const String EditorAssets::FolderIcon = "Resources/Icons/Folder.asset";

    const String EditorAssets::ArrowPointerIcon = "Resources/Icons/ArrowPointerIcon.asset";
    const String EditorAssets::ArrowsIcon = "Resources/Icons/ArrowsIcon.asset";
    const String EditorAssets::RotateIcon = "Resources/Icons/RotateIcon.asset";
    const String EditorAssets::MaximizeIcon = "Resources/Icons/MaximizeIcon.asset";
    const String EditorAssets::GlobeIcon = "Resources/Icons/GlobeIcon.asset";
    const String EditorAssets::SearchIcon = "Resources/Icons/SearchIcon.asset";

    const String EditorAssets::ConsoleInfo = "Resources/Icons/ConsoleInfo.asset";
    const String EditorAssets::ConsoleWarn = "Resources/Icons/ConsoleWarn.asset";
    const String EditorAssets::ConsoleError = "Resources/Icons/ConsoleError.asset";

    const String EditorAssets::AlignLeft = "Resources/Icons/AlignLeft.asset";
    const String EditorAssets::AlignCenter = "Resources/Icons/AlignCenter.asset";
    const String EditorAssets::AlignRight = "Resources/Icons/AlignRight.asset";

    const String EditorAssets::DefaultScriptPath = "Resources/Default/DefaultScript.cs";

    EditorAssetsLibrary EditorAssets::s_Library;
    String EditorAssets::s_DefaultScriptTemplate;

    static Ref<Texture> LoadTexture(const Path& path)
    {
        const AssetHandle<Texture> texture = AssetManager::TryGet()->Load<Texture>(path);
        return texture ? texture.GetInternalPtr() : nullptr;
    }

    void EditorAssets::Load()
    {
        s_Library.UnassignedTexture = Texture::MISSING;

        s_Library.PlayIcon = LoadTexture(PlayIcon);
        s_Library.StopIcon = LoadTexture(StopIcon);
        s_Library.PauseIcon = LoadTexture(PauseIcon);

        s_Library.FolderIcon = LoadTexture(FolderIcon);
        s_Library.FileIcon = LoadTexture(FileIcon);

        s_Library.ArrowPointerIcon = LoadTexture(ArrowPointerIcon);
        s_Library.ArrowsIcon = LoadTexture(ArrowsIcon);
        s_Library.RotateIcon = LoadTexture(RotateIcon);
        s_Library.MaximizeIcon = LoadTexture(MaximizeIcon);
        s_Library.GlobeIcon = LoadTexture(GlobeIcon);
        s_Library.SearchIcon = LoadTexture(SearchIcon);
        s_Library.SettingsIcon = CreateSettingsIcon();

        s_Library.ConsoleInfo = LoadTexture(ConsoleInfo);
        s_Library.ConsoleWarn = LoadTexture(ConsoleWarn);
        s_Library.ConsoleError = LoadTexture(ConsoleError);

        s_Library.AlignLeft = LoadTexture(AlignLeft);
        s_Library.AlignCenter = LoadTexture(AlignCenter);
        s_Library.AlignRight = LoadTexture(AlignRight);

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Library.Test = StaticRefCast<Font>(font)->GetAtlasTexture();
    }

    const String& EditorAssets::GetDefaultScriptTemplate()
    {
        if (s_DefaultScriptTemplate.empty())
            s_DefaultScriptTemplate = FileSystem::ReadTextFile(DefaultScriptPath);
        return s_DefaultScriptTemplate;
    }

    void EditorAssets::Unload()
    {
        s_Library.UnassignedTexture = nullptr;
        s_Library.PlayIcon = nullptr;
        s_Library.StopIcon = nullptr;
        s_Library.PauseIcon = nullptr;
        s_Library.FolderIcon = nullptr;
        s_Library.FileIcon = nullptr;
        s_Library.ArrowPointerIcon = nullptr;
        s_Library.ArrowsIcon = nullptr;
        s_Library.RotateIcon = nullptr;
        s_Library.MaximizeIcon = nullptr;
        s_Library.GlobeIcon = nullptr;
        s_Library.SearchIcon = nullptr;
        s_Library.SettingsIcon = nullptr;
        s_Library.ConsoleInfo = nullptr;
        s_Library.ConsoleWarn = nullptr;
        s_Library.ConsoleError = nullptr;
        s_Library.AlignLeft = nullptr;
        s_Library.AlignCenter = nullptr;
        s_Library.AlignRight = nullptr;
        s_DefaultScriptTemplate.clear();
    }
} // namespace Crowny
