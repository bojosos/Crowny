#include "cwpch.h"

#include "Platform/OpenGL/OpenGLInfo.h"

#include <glad/glad.h>

namespace Crowny
{

    Vector<OpenGLDetail> OpenGLInfo::s_Information;

    Vector<OpenGLDetail>& OpenGLInfo::GetInformation() { return s_Information; }

    void OpenGLInfo::RetrieveInformation()
    {
        s_Information.push_back({ "Vendor", "GL_VENDOR", String((char*)glGetString(GL_VENDOR)) });
        s_Information.push_back({ "Renderer", "GL_RENDERER", String((char*)glGetString(GL_RENDERER)) });
        s_Information.push_back({ "Version", "GL_VERSION", String((char*)glGetString(GL_VERSION)) });
        s_Information.push_back(
          { "GLSL Version", "GL_SHADING_LANGUAGE_VERSION", String((char*)glGetString(GL_SHADING_LANGUAGE_VERSION)) });

        int maxTexture2DSize;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture2DSize);
        s_Information.push_back({ "Max Texture2D Size", "GL_MAX_TEXTURE_SIZE",
                                  std::to_string(maxTexture2DSize) + "x" + std::to_string(maxTexture2DSize) });

        int maxTexture3DSize;
        glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxTexture3DSize);
        s_Information.push_back({ "Max Texture3D Size", "GL_MAX_3D_TEXTURE_SIZE",
                                  std::to_string(maxTexture3DSize) + "x" + std::to_string(maxTexture3DSize) + "x" +
                                    std::to_string(maxTexture3DSize) });

        int maxElements;
        glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &maxElements);
        s_Information.push_back({ "Max Elements", "GL_MAX_ELEMENTS_INDICES", std::to_string(maxElements) });

        int maxVertices;
        glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &maxVertices);
        s_Information.push_back({ "Max Vertices", "GL_MAX_ELEMENTS_VERTICES", std::to_string(maxVertices) });

        int maxTextureSlots;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureSlots);
        s_Information.push_back({ "Max Texture Slots", "GL_MAX_TEXTURE_IMAGE_UNITS", std::to_string(maxTextureSlots) });

        int maxCombinedTextures;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTextures);
        s_Information.push_back(
          { "Max Combined Texture Slots", "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS", std::to_string(maxCombinedTextures) });
    }
} // namespace Crowny