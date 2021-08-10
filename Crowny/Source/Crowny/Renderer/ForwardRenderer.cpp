/*#include "cwpch.h"

#include "Crowny/Renderer/ForwardRenderer.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"
#include "Crowny/RenderAPI/RenderCommand.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{
    struct ForwardRendererData
    {
        Ref<Shader> BackgroundShader = nullptr;
       // Ref<EnvironmentMap> Envmap = nullptr;
    };

    enum VSSystemUniformIndices : int32_t
    {
        VSSystemUniformIndex_ProjectionMatrix = 0,
        VSSystemUniformIndex_ViewMatrix = 1,
        VSSystemUniformIndex_ModelMatrix = 2,
        VSSystemUniformIndex_CameraPosition = 3,
        VSSystemUniformIndex_Size
    };

    enum FSSystemUniformIndices : int32_t
    {
        FSSystemUniformIndex_Lights = 0,
        FSSystemUniformIndex_Size
    };

    static ForwardRendererData s_Data;

    void ForwardRenderer::Init()
    {
        s_Data.BackgroundShader = Shader::Create("/Shaders/Background.glsl");
        //s_Data.Envmap = EnvironmentMap::Create("/Textures/envmap.hdr");
    }

    void ForwardRenderer::Begin()
    {

    }

    void ForwardRenderer::BeginScene(const Camera& camera, const glm::mat4& viewMatrix)
    {
        memcpy(data, glm::value_ptr(camera.GetProjection()), sizeof(glm::mat4));
        memcpy(data + sizeof(glm::mat4), glm::value_ptr(viewMatrix), sizeof(glm::mat4));


        memcpy(s_Data.VSSystemUniformBuffer +
s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix], glm::value_ptr(camera.GetProjection()),
sizeof(glm::mat4)); memcpy(s_Data.VSSystemUniformBuffer +
s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ViewMatrix], glm::value_ptr(viewMatrix), sizeof(glm::mat4)); //
no inverse? glm::vec3 pos = glm::vec3(glm::inverse(viewMatrix)[3]); memcpy(s_Data.VSSystemUniformBuffer +
s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_CameraPosition], glm::value_ptr(pos), sizeof(glm::vec3));
    }

    void ForwardRenderer::SetSystemUniforms(const Ref<Shader>& shader)
    {
        shader->SetVSSystemUniformBuffer(s_Data.VSSystemUniformBuffer, s_Data.VSSystemUniformBufferSize, 0);
        shader->SetFSSystemUniformBuffer(s_Data.FSSystemUniformBuffer, s_Data.FSSystemUniformBufferSize, 0);
    }

    void ForwardRenderer::SubmitLightSetup()
    {
        glm::vec3 lightPositions[] = {
            glm::vec3(-10.0f,  10.0f, 10.0f),
            glm::vec3(10.0f,  10.0f, 10.0f),
            glm::vec3(-10.0f, -10.0f, 10.0f),
            glm::vec3(10.0f, -10.0f, 10.0f),
        };
        glm::vec3 lightColors[] = {
            glm::vec3(300.0f, 300.0f, 300.0f),
            glm::vec3(300.0f, 300.0f, 300.0f),
            glm::vec3(300.0f, 300.0f, 300.0f),
            glm::vec3(300.0f, 300.0f, 300.0f)
        };

        ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightPositions", lightPositions);
        ImGuiMaterialPanel::GetSlectedMaterial()->SetUniform("lightColors", lightColors);
    }

    void ForwardRenderer::Submit(const Ref<Model>& model)
    {
        for (auto& mesh : model->GetMeshes())
        {
            model->Draw();
        }
    }

    void ForwardRenderer::SubmitMesh(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        mesh->GetMaterialInstance()->Bind(3);
        memcpy(s_Data.VSSystemUniformBuffer + s_Data.VSSystemUniformBufferOffsets[VSSystemUniformIndex_ModelMatrix],
glm::value_ptr(transform), sizeof(glm::mat4));
        SetSystemUniforms(mesh->GetMaterialInstance()->GetMaterial()->GetShader());
        mesh->GetVertexArray()->Bind();
        SubmitLightSetup();
        RenderCommand::DrawIndexed(mesh->GetVertexArray());
    }

    void ForwardRenderer::EndScene()
    {
        static unsigned int cubeVAO = 0;
        static unsigned int cubeVBO = 0;
        if (cubeVAO == 0)
        {
            float vertices[] = {
                // back face
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                // front face
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                // left face
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                // right face
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left
                // bottom face
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                // top face
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left
            };
            glGenVertexArrays(1, &cubeVAO);
            glGenBuffers(1, &cubeVBO);
            // fill buffer
            glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            // link vertex attributes
            glBindVertexArray(cubeVAO);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

    void ForwardRenderer::End()
    {

    }

    void ForwardRenderer::Flush()
    {

    }

}*/