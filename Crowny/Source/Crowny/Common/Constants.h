#pragma once

namespace Crowny
{

	// ------------- PATHS -------------
#ifdef CW_WEB
	#define DIRECTORY_PREFIX "Minecraft/res/"
#else
	#define DIRECTORY_PREFIX "res/"
#endif

#define DEFAULT_FONT_PATH "fonts/roboto-thin.ttf"

	// ------------- BATCH RENDERER -------------
#define RENDERER_MAX_SPRITES	60000
#define RENDERER_VERTEX_SIZE	sizeof(VertexData)
#define RENDERER_SPRITE_SIZE	RENDERER_VERTEX_SIZE * 4
#define RENDERER_BUFFER_SIZE	RENDERER_SPRITE_SIZE * RENDERER_MAX_SPRITES
#define RENDERER_INDICES_SIZE	RENDERER_MAX_SPRITES * 6
#define MAX_TEXTURE_SLOTS       32 // TODO: Check for these
}