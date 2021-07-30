#pragma once

#include "ImGuiPanel.h"

#include <filesystem>
#include <imgui.h>

namespace Crowny
{
	
	enum class AssetBrowserItem
	{
		Folder,
		CScript,
		Scene,
		Prefab,
		Material,
		Texture,
		RenderTexture,
		Shader,
		ComputeShader,
		PhysicsMaterial
	};

	class ImGuiAssetBrowserPanel : public ImGuiPanel
	{
	public:
		ImGuiAssetBrowserPanel(const std::string& name);
		~ImGuiAssetBrowserPanel() = default;

		virtual void Render() override;

		virtual void Show() override;
		virtual void Hide() override;

	private:
		void ShowContextMenuContents(const std::string& filepath = "");
	private:
		ImTextureID m_FolderIcon;
		ImTextureID m_FileIcon;
		
		std::unordered_map<std::string, Ref<Texture>> m_Textures; // For showing the textures in the asset browser.
		std::unordered_set<std::string> m_SelectedFiles;
		std::filesystem::path m_PreviousDirectory;
		std::filesystem::path m_CurrentDirectory;
		
		std::string m_Filename;
		AssetBrowserItem m_RenamingType;
		std::filesystem::path m_RenamingPath;
	};

}