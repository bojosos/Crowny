#include "cwpch.h"

#include "Platform/OpenGL/OpenGLShader.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    OpenGLShader::~OpenGLShader() { glDeleteProgram(m_RendererID); }

} // namespace Crowny