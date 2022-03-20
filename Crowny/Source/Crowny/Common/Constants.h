#pragma once

namespace Crowny
{

#define DEFAULT_FONT_FILENAME String("roboto-thin.ttf")

    // ------------- Global Renderer -------------
#define MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 8
#define MAX_QUEUES_PER_TYPE 8
#define MAX_BOUND_VERTEX_BUFFERS 16

#define RENDERER2D_SHADER_PATH "Resources/Shaders/BatchRenderer.asset"
#define BRDF_SHADER_PATH "Resources/Shaders/Brdf.asset"
#define PREFILTER_SHADER_PATH "Resources/Shaders/Prefilter.asset"
#define FILTER_SHADER_PATH "Resources/Shaders/Filter.asset"
#define SKYBOX_SHADER_PATH "Resources/Shaders/Skybox.asset"
#define PBRIBL_SHADER_PATH "Resources/Shaders/Pbribl.asset"
#define EQUIRECTTOCUBE_SHADER_PATH "Resources/Shaders/EquirectToCube.asset"

    // ------------- Batch Renderer -------------
#define RENDERER_MAX_SPRITES 60000
#define RENDERER_VERTEX_SIZE sizeof(VertexData)
#define RENDERER_SPRITE_SIZE RENDERER_VERTEX_SIZE * 4
#define RENDERER_BUFFER_SIZE RENDERER_SPRITE_SIZE* RENDERER_MAX_SPRITES
#define RENDERER_INDICES_SIZE RENDERER_MAX_SPRITES * 6
#define MAX_TEXTURE_SLOTS 32 // TODO: Check for these, done just assign

} // namespace Crowny