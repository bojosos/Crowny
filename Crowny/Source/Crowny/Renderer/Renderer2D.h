#pragma once

#include "Crowny/Common/Color.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"

namespace Crowny
{
    struct VertexData
    {
        glm::vec4 Position;
        glm::vec2 Uv;
        float Tid;
        glm::vec4 Color;
    };

    class Renderer2D
    {
    public:
        /**
         * @brief Initializes the renderer.
         *
         */
        static void Init();

        /**
         * @brief Frees any resources used by the renderer.
         *
         */
        static void Shutdown();

    private:
        /**
         * @brief Searches for a texture in the cached textures.
         * Returns the texture idx in our array. If there is no space in the
         * array the current batch is submitted and new one is started.
         *
         * @param texture Texture we are looking for
         * @return float The texture id
         */
        static float FindTexture(const Ref<Texture>& texture);

        /**
         * @brief Draws the triangles on the screen.
         *
         */
        static void Flush();

    public:
        /**
         * @brief Draws a filled Rectangle.
         *
         * @param transform The transform of the rectangle.
         * @param texture Texture to use.
         * @param color Color to draw with.
         */
        static void FillRect(const glm::mat4& transform, const Ref<Texture>& texture, const glm::vec4& color);

        /**
         * @brief Draws a filled Rectangle.
         *
         * @param bounds Bounds of the rectangle.
         * @param color Color to draw with.
         */
        static void FillRect(const Rect2F& bounds, const glm::vec4& color);

        /**
         * @brief Draws a filled rectangle.
         *
         * @param bounds Bounds of the rectangle.
         * @param texture Texture to use.
         * @param color Color to draw with.
         */
        static void FillRect(const Rect2F& bounds, const Ref<Texture>& texture, const glm::vec4& color);

        /**
         * @brief Draws a string on the screen.
         *
         * @param text Text to draw.
         * @param x X-coord of the text.
         * @param y Y-coord of the text.
         * @param font Font to use.
         * @param color Color draw with.
         */
        static void DrawString(const std::string& text, float x, float y, const Ref<Font>& font,
                               const glm::vec4& color);

        /**
         * @brief Draws a string on the screen.
         *
         * @param text Text to draw.
         * @param transform Transform component.
         * @param font Font to use.
         * @param color Color to draw with.
         */
        static void DrawString(const std::string& text, const glm::mat4& transform, const Ref<Font>& font,
                               const glm::vec4& color);

        /**
         * @brief Begin a batch.
         * Uploads uniform buffers and sets rendering state.
         *
         * @param camera Camera to render from.
         * @param viewMatrix View matrix of the scene.
         */
        static void Begin(const Camera& camera, const glm::mat4& viewMatrix);

        /**
         * @brief Ends a batch.
         * Flushes the batched quads and draws them on the screen.
         */
        static void End();
    };
} // namespace Crowny