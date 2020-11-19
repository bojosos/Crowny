#include "cwepch.h"

#include "Editor/EditorAssets.h"

namespace Crowny
{
    Ref<Texture2D> EditorAssets::UnassignedTexture = nullptr;

	void EditorAssets::LoadAssets()
	{
		UnassignedTexture = Texture2D::Create("/Textures/Unassigned.png");
	}
}