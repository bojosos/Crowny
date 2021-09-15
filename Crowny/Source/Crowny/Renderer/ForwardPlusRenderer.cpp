#include "cwpch.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
/*
namespace Crowny
{

    struct PointLight {
        glm::vec4 color;
        glm::vec4 position;
        glm::vec4 paddingAndRadius;
    };

    struct VisibleIndex {
        int index;
    };

    struct ForwardPlusRendererData
    {
        Ref<Shader> DepthShader;
        Ref<Shader> LightCullingShader;
        Ref<Shader> LightAccumulationShader;
        Ref<Shader> HdrShader;

        Ref<Shader> DepthDebugShader;

        uint32_t LightBuffer;
        uint32_t VisibleLightIndicesBuffer;

        uint32_t RboDepth;
        uint32_t DepthMapFBO;
        uint32_t HdrFBO;

        uint32_t DepthMap;
        uint32_t ColorBuffer;

        glm::vec2 ScreenSize;
        uint32_t WorkGroupsX, WorkGroupsY;
    };

    static ForwardPlusRendererData s_Data;
    static uint32_t quadVAO = 0; // temp
    static uint32_t quadVBO = 0;

    const glm::vec3 LIGHT_MIN_BOUNDS = glm::vec3(-135.0f, -20.0f, -60.0f); // temp
    const glm::vec3 LIGHT_MAX_BOUNDS = glm::vec3(135.0f, 170.0f, 60.0f);
    const float LIGHT_DELTA_TIME = -0.6f;
    const unsigned int NUM_LIGHTS = 1024;
    const float LIGHT_RADIUS = 30.0f;

    std::vector<MeshTran> ForwardPlusRenderer::s_Meshes = {};

    static glm::vec3 RandomPosition(std::uniform_real_distribution<> dis, std::mt19937 gen) {
        glm::vec3 position = glm::vec3(0.0);
        for (int i = 0; i < 3; i++) {
            float min = LIGHT_MIN_BOUNDS[i];
            float max = LIGHT_MAX_BOUNDS[i];
            position[i] = (GLfloat)dis(gen) * (max - min) + min;
        }

        return position;
    }

    static void SetupLights() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_Data.LightBuffer);
        PointLight *pointLights = (PointLight*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

        for (int i = 0; i < NUM_LIGHTS; i++) {
            PointLight &light = pointLights[i];
            light.position = glm::vec4(RandomPosition(dis, gen), 1.0f);
            light.color = glm::vec4(1.0f + dis(gen), 1.0f + dis(gen), 1.0f + dis(gen), 1.0f);
            light.paddingAndRadius = glm::vec4(glm::vec3(0.0f), LIGHT_RADIUS);
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    static void UpdateLights() {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_Data.LightBuffer);
        PointLight *pointLights = (PointLight*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);

        for (int i = 0; i < NUM_LIGHTS; i++) {
            PointLight &light = pointLights[i];
            float min = LIGHT_MIN_BOUNDS[1];
            float max = LIGHT_MAX_BOUNDS[1];

            light.position.y = fmod((light.position.y + (-4.5f * 0.03) - min + max), max) + min;
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    static void DrawQuad()
    {
        if (quadVAO == 0) {
            GLfloat quadVertices[] = {
                -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };

            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        }

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    void ForwardPlusRenderer::Init()
    {
        s_Data.ScreenSize = { 789.0f, 470.0f }; // temp
        s_Data.WorkGroupsX = ((int32_t)s_Data.ScreenSize.x + ((int32_t)s_Data.ScreenSize.x % 16)) / 16;
        s_Data.WorkGroupsY = ((int32_t)s_Data.ScreenSize.y + ((int32_t)s_Data.ScreenSize.y % 16)) / 16;
        size_t numberOfTiles = s_Data.WorkGroupsX * s_Data.WorkGroupsY;
        // TODO: Make these directories relative to root directory of project
        s_Data.DepthShader = Shader::Create("/Shaders/Depth.glsl");
        s_Data.LightCullingShader = Shader::Create("/Shaders/LightCulling.glsl");
        s_Data.LightAccumulationShader = Shader::Create("/Shaders/LightAccumulation.glsl");
        s_Data.HdrShader = Shader::Create("/Shaders/Hdr.glsl");
        s_Data.DepthDebugShader = Shader::Create("/Shaders/DepthDebug.glsl");

        glGenBuffers(1, &s_Data.LightBuffer);
        glGenBuffers(1, &s_Data.VisibleLightIndicesBuffer);

        // Bind light buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_Data.LightBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_LIGHTS * sizeof(PointLight), 0, GL_DYNAMIC_DRAW);

        // Bind visible light indices buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_Data.VisibleLightIndicesBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numberOfTiles * sizeof(VisibleIndex) * 1024, 0, GL_STATIC_DRAW);

        // Set the default values for the light buffer
        SetupLights();

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glGenFramebuffers(1, &s_Data.DepthMapFBO);

        glGenTextures(1, &s_Data.DepthMap);
        glBindTexture(GL_TEXTURE_2D, s_Data.DepthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, s_Data.ScreenSize.x, s_Data.ScreenSize.y, 0,
GL_DEPTH_COMPONENT, GL_FLOAT, NULL); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, s_Data.DepthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_Data.DepthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glGenFramebuffers(1, &s_Data.HdrFBO);

        glGenTextures(1, &s_Data.ColorBuffer);
        glBindTexture(GL_TEXTURE_2D, s_Data.ColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, s_Data.ScreenSize.x, s_Data.ScreenSize.y, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenRenderbuffers(1, &s_Data.RboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, s_Data.RboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, s_Data.ScreenSize.x, s_Data.ScreenSize.y);

        glBindFramebuffer(GL_FRAMEBUFFER, s_Data.HdrFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_Data.ColorBuffer, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_Data.RboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));
        s_Data.DepthShader->Bind();
       // s_Data.DepthShader->SetUniformMat4("model", model);

        s_Data.LightCullingShader->Bind();
       // s_Data.LightCullingShader->SetUniformInt("lightCount", 1024);
       // s_Data.LightCullingShader->SetUniformInt2("screenSize", (int32_t)s_Data.ScreenSize.x,
(int32_t)s_Data.ScreenSize.y);

        s_Data.DepthDebugShader->Bind();
        //s_Data.DepthDebugShader->SetUniformMat4("model", model);
       // s_Data.DepthDebugShader->SetUniformFloat("near", 0.1f);
        //s_Data.DepthDebugShader->SetUniformFloat("far", 300.0f);

        s_Data.LightAccumulationShader->Bind();
        //s_Data.LightAccumulationShader->SetUniformMat4("model", model);
        //s_Data.LightAccumulationShader->SetUniformInt("numberOfTilesX", s_Data.WorkGroupsX);

        glViewport(0, 0, s_Data.ScreenSize.x, s_Data.ScreenSize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    void ForwardPlusRenderer::BeginFrame(const EditorCamera& camera)
    {
        s_Data.LightAccumulationShader->Bind();
      //  s_Data.LightAccumulationShader->SetUniformMat4("projection", camera.GetProjection());
     //   s_Data.LightAccumulationShader->SetUniformMat4("view", camera.GetViewMatrix());
       // s_Data.LightAccumulationShader->SetUniformFloat3("viewPosition", camera.GetPosition());

        s_Data.LightCullingShader->Bind();
        //s_Data.LightCullingShader->SetUniformMat4("projection", camera.GetProjection());
        //s_Data.LightCullingShader->SetUniformMat4("view", camera.GetViewMatrix());

        s_Data.DepthShader->Bind();
        //s_Data.DepthShader->SetUniformMat4("projection", camera.GetProjection());
        //s_Data.DepthShader->SetUniformMat4("view", camera.GetViewMatrix());

        s_Data.DepthDebugShader->Bind();
      //  s_Data.DepthDebugShader->SetUniformMat4("projection", camera.GetProjection());
       // s_Data.DepthDebugShader->SetUniformMat4("view", camera.GetViewMatrix());

        UpdateLights();
    }

    void ForwardPlusRenderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
    {
        s_Meshes.push_back({ mesh, transform });
    }

    void ForwardPlusRenderer::Submit(const Ref<Model>& model, const glm::mat4& transform)
    {

    }

    void ForwardPlusRenderer::EndFrame()
    {
        s_Data.DepthShader->Bind();
        glBindFramebuffer(GL_FRAMEBUFFER, s_Data.DepthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        for (auto& mt : s_Meshes)
        {
            mt.mesh->Draw();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        s_Data.LightCullingShader->Bind();

        glBindTextureUnit(0, s_Data.DepthMap);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_Data.LightBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, s_Data.VisibleLightIndicesBuffer);
        glDispatchCompute(s_Data.WorkGroupsX, s_Data.WorkGroupsY, 1);

        glBindTextureUnit(0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, s_Data.HdrFBO);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        s_Data.LightAccumulationShader->Bind();
        for (auto& mt : s_Meshes)
        {
          //  s_Data.LightAccumulationShader->SetUniformMat4("model", mt.tran);
            mt.mesh->Draw();
        }
        s_Data.HdrShader->Bind();
        glBindTextureUnit(0, s_Data.ColorBuffer);
        float exposure = 1.0f;
       // s_Data.HdrShader->SetUniformFloat("exposure", exposure);
        DrawQuad();
        glBindTextureUnit(0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        s_Meshes.clear();
    }

    void ForwardPlusRenderer::Shutdown()
    {

    }

}*/