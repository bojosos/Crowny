#pragma once

#include "Crowny/Common/Color.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexArray.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Font.h"

namespace Crowny
{

    /**
     * @brief General purpose renderer for rendering 2D objects (text, quads).
     *
     */
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
        static void FillRect(const glm::mat4& transform, const Ref<Texture>& texture, const glm::vec4& color,
                             uint32_t entityId);

        /**
         * @brief Draws a filled Rectangle.
         *
         * @param bounds Bounds of the rectangle.
         * @param color Color to draw with.
         */
        static void FillRect(const Rect2F& bounds, const glm::vec4& color, uint32_t entityId);

        /**
         * @brief Draws a filled rectangle.
         *
         * @param bounds Bounds of the rectangle.
         * @param texture Texture to use.
         * @param color Color to draw with.
         */
        static void FillRect(const Rect2F& bounds, const Ref<Texture>& texture, const glm::vec4& color,
                             uint32_t entityId);

        /**
         * @brief Draws a circle. Thickness controls how 'filled' the circle is. 1.0 means it is full and 0.0 no circle
         * at all.
         *
         *
         * @param transform Transform of the circle.
         * @param color Color of the cirlce.
         * @param thickness Thickness.
         * @param fade Fade.
         * @param entityId Id of the object for mouse picking.
         */
        static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness = 1.0f,
                               float fade = 0.005f, int32_t entityId = -1);

        /**
         * @brief Draws a line.
         *
         * @param p1 Starting point.
         * @param p2 End point.
         * @param color Color of the line.
         * @param thickness Thickness.
         */
        static void DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color, float thickness = 0.02f);

        static void DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color,
                             float thickness = 0.02f);

        static void DrawRect(const glm::mat4& transform, const glm::vec4& color, float thickness = 0.02f);

        /**
         * @brief Draws a quad on the screen from (0, 0) to (1, 1)
         *
         * @param uvs The uvs of the texture we want to use.
         */
        static void DrawScreenQuad(const Rect2F& uvs);

        /**
         * @brief Draws a quad on the screen from (0, 0) to (1, 1)
         */
        static void DrawScreenQuad();

        /**
         * @brief Draws a string on the screen.
         *
         * @param text Text to draw.
         * @param x X-coord of the text.
         * @param y Y-coord of the text.
         * @param font Font to use.
         * @param color Color draw with.
         */
        static void DrawString(const String& text, float x, float y, const Ref<Font>& font, const glm::vec4& color);

        /**
         * @brief Draws a string on the screen.
         *
         * @param text Text to draw.
         * @param transform Transform component.
         * @param font Font to use.
         * @param color Color to draw with.
         */
        static void DrawString(const String& text, const glm::mat4& transform, const Ref<Font>& font,
                               const glm::vec4& color);

        /**
         * @brief Begin a batch.
         * Uploads uniform buffers and sets rendering state.
         *
         * @param camera Camera to render from.
         * @param viewMatrix View matrix of the scene.
         */
        static void Begin(const Camera& camera, const glm::mat4& viewMatrix);

        static void Begin(const glm::mat4& projection, const glm::mat4& view);

        /**
         * @brief Ends a batch.
         * Flushes the batched quads and draws them on the screen.
         */
        static void End();
    };
} // namespace Crowny