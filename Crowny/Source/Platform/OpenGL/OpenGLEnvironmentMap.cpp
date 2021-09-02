#include "cwpch.h"

#include "Platform/OpenGL/OpenGLEnvironmentMap.h"
#include "Crowny/RenderAPI/RenderTexture.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Timer.h"

#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace Crowny
{

    float vertices[] = {
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f,  1.0f, -1.0f,
        1.0f,  1.0f,  1.0f,
        1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f
    };

    uint32_t indices[] = {
        0,  1,  2, 0,  2,  3,
        4,  5,  6, 4,  6,  7,
        8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };
    
    OpenGLEnvironmentMap::OpenGLEnvironmentMap(const std::string& filepath)
        {
        // Load equirectangular map
        stbi_set_flip_vertically_on_load(true);
        auto [dat, size] = VirtualFileSystem::Get()->ReadFile(filepath);
        int width, height, channels;
        float* data = stbi_loadf_from_memory(dat, size, &width, &height, &channels, 0);
        if (data)
        {
            m_Width = width;
            m_Height = height;
            m_Channels = channels;
            TextureParameters tProps;
            tProps.Width = m_Width;
            tProps.Height = m_Height;
            tProps.Usage = TextureUsage::TEXTURE_STATIC;
            tProps.Format = TextureFormat::RGBA32F;
            m_Texture = Texture::Create(tProps);
            PixelData dat(m_Width, m_Height, 1, TextureFormat::RGB32F);
            dat.SetBuffer((uint8_t*)data);
            m_Texture->WriteData(dat);
            dat.SetBuffer(nullptr);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Failed to load HDR image." << std::endl;
        }
        delete dat;

        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };
        auto& rapi = RenderAPI::Get();
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.NumArraySlices = tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.Format = TextureFormat::RGBA32F;
        
        m_Envmap = Texture::Create(tProps);
        
        // Convert HDR equirectangular environment map to cubemap
        Ref<Shader> eqToCubeVert = Shader::Create(ShaderCompiler().Compile("/Shaders/EqToCube.vert", ShaderType::VERTEX_SHADER));
        Ref<Shader> eqToCubeFrag = Shader::Create(ShaderCompiler().Compile("/Shaders/EqToCube.frag", ShaderType::FRAGMENT_SHADER));
        PipelineStateDesc desc;
        desc.VertexShader = eqToCubeVert;
        desc.FragmentShader = eqToCubeFrag;
        BufferLayout layout = { { ShaderDataType::Float3, "a_Position" } };
        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, layout);

        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);
        Ref<UniformBufferBlock> block = UniformBufferBlock::Create(eqToCubeVert->GetUniformDesc()->Uniforms.at("VP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "VP", block);
        uniforms->SetTexture(0, 1, m_Texture);
        block->Write(0, &captureProjection, sizeof(glm::mat4));

        m_SkyboxVbo = VertexBuffer::Create(vertices, 72 * sizeof(float));
        m_SkyboxIbo = IndexBuffer::Create(indices, 36);
        m_FilterLayout = { { ShaderDataType::Float3, "a_Position" } };
        m_SkyboxVbo->SetLayout(m_FilterLayout);
        
        rapi.SetViewport(0, 0, 512, 512);
        rapi.SetGraphicsPipeline(pipeline);

        for (uint32_t i = 0; i < 6; i++)
        {
            RenderTextureProperties rtProps;
            rtProps.ColorSurfaces[0].Texture = m_Envmap;
            rtProps.ColorSurfaces[0].Face = i;
            rtProps.ColorSurfaces[0].NumFaces = 1;
            rtProps.ColorSurfaces[0].MipLevel = 0;
            rtProps.Width = tProps.Width;
            rtProps.Height = tProps.Height;
            Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
            rapi.SetRenderTarget(cubemap);
            
            block->Write(sizeof(glm::mat4), &captureViews[i], sizeof(glm::mat4));
            rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
            rapi.SetIndexBuffer(m_SkyboxIbo);
            rapi.SetUniforms(uniforms);
            rapi.DrawIndexed(0, 36, 0, 72);
        }

        GenerateBRDFLUT();
        GeneratePrefilteredCube();
        GenerateIrradianceCube();
    }

    void OpenGLEnvironmentMap::GenerateBRDFLUT()
    {
        Timer t;
        auto& rapi = RenderAPI::Get();
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Format = TextureFormat::RG32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        Ref<Texture> brdf = Texture::Create(tProps);
        
        RenderTextureProperties rtProps;
        rtProps.Width = tProps.Width;
        rtProps.Height = tProps.Height;
        rtProps.ColorSurfaces[0] = { brdf };
        static Ref<RenderTexture> target = RenderTexture::Create(rtProps);

        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/Brdf.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/Brdf.frag", FRAGMENT_SHADER));
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, {});
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        rapi.SetRenderTarget(target);
        rapi.SetGraphicsPipeline(pipeline);
        rapi.SetViewport(0, 0, 512, 512);
        rapi.SetUniforms(uniforms);
        rapi.Draw(0, 3, 1);
        
        CW_ENGINE_INFO("BRDFLut generation took: {0}", t.ElapsedSeconds());
    }

    void OpenGLEnvironmentMap::GenerateIrradianceCube()
    {
        Timer timer;
        auto& rapi = RenderAPI::Get();
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(64)));
        TextureParameters tProps;
        tProps.Width = 64;
        tProps.Height = 64;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = tProps.NumArraySlices = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        Ref<Texture> texture = Texture::Create(tProps);
        
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/filter.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/irradiance.frag", FRAGMENT_SHADER));
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, m_FilterLayout);
        Rect2F viewport = { 0.0f, 0.0f, 64.0f, 64.0f };
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        std::vector<glm::mat4> matrices = {
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // NEGATIVE_X
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };
        
        Ref<UniformBufferBlock> mvp = UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms = UniformParams::Create(pipeline);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", mvp);
        uniforms->SetTexture(0, 1, m_Envmap);
        for (uint32_t j = 0; j < 6; j++)
        {
            auto persp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 64.0f) * matrices[j];
            mvp->Write(0, &persp, sizeof(glm::mat4));
            for (uint32_t i = 0; i < numMips; i++)
            {
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = texture;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / std::pow(2, i);
                rtProps.Height = tProps.Height / std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(pipeline);
                viewport.Width = static_cast<float>(64 * std::pow(0.5f, i));
                viewport.Height = static_cast<float>(64 * std::pow(0.5f, i));
                rapi.SetViewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
                rapi.SetUniforms(uniforms);
                rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
                rapi.SetIndexBuffer(m_SkyboxIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
        CW_ENGINE_INFO("Irrdiance cube took: {0}", timer.ElapsedSeconds());
    }

    void OpenGLEnvironmentMap::GeneratePrefilteredCube()
    {
        Timer timer;

        struct PrefilterParams
        {
            uint32_t samples = 32;
            float roughness;
        } params;

        auto& rapi = RenderAPI::Get();
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(64)));
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = tProps.NumArraySlices = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        Ref<Texture> texture = Texture::Create(tProps);
        
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/filter.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/prefilter.frag", FRAGMENT_SHADER));
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, m_FilterLayout);
        Rect2F viewport = { 0.0f, 0.0f, 512.0f, 512.0f };
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        std::vector<glm::mat4> matrices = {
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // NEGATIVE_X
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };
        
        Ref<UniformBufferBlock> mvp = UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms = UniformParams::Create(pipeline);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", mvp);
        uniforms->SetTexture(0, 1, m_Envmap);
        Ref<UniformBufferBlock> fragBlock = UniformBufferBlock::Create(fragment->GetUniformDesc()->Uniforms.at("Params").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms->SetUniformBlockBuffer(ShaderType::FRAGMENT_SHADER, "Params", fragBlock);
        for (uint32_t j = 0; j < 6; j++)
        {
            auto persp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[j];
            mvp->Write(0, &persp, sizeof(glm::mat4));
            for (uint32_t i = 0; i < numMips; i++)
            {
                params.roughness = (float)i / (float)(numMips);
                fragBlock->Write(0, &params, sizeof(PrefilterParams));
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = texture;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / std::pow(2, i);
                rtProps.Height = tProps.Height / std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(pipeline);
                viewport.Width = static_cast<float>(512 * std::pow(0.5f, i));
                viewport.Height = static_cast<float>(512 * std::pow(0.5f, i));
                rapi.SetViewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
                rapi.SetUniforms(uniforms);
                rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
                rapi.SetIndexBuffer(m_SkyboxIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
        
        CW_ENGINE_INFO("Prefiltered cube took: {0}", timer.ElapsedSeconds());
    }
    
    static void Draw()
    {/*
        static unsigned int cubeVAO1 = 0;
        static unsigned int cubeVBO1 = 0;
        if (cubeVAO1 == 0)
        {
            
            glGenVertexArrays(1, &cubeVAO1);
            glGenBuffers(1, &cubeVBO1);
            // fill buffer
            glBindBuffer(GL_ARRAY_BUFFER, cubeVBO1);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            // link vertex attributes
            glBindVertexArray(cubeVAO1);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        glBindVertexArray(cubeVAO1);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);*/
    }

    static void RenderQuad()
    {/*
        static unsigned int quadVAO = 0;
        static unsigned int quadVBO;
        if (quadVAO == 0)
        {
            float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };
            // setup plane VAO
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);*/
    }

    void OpenGLEnvironmentMap::ToCubemap()
    {/*
        // glEnable(GL_DEPTH_TEST);
        // glDepthFunc(GL_LEQUAL);
        // glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        auto& rapi = RenderAPI::Get();
        
        unsigned int captureFBO;
        unsigned int captureRBO;
        glGenFramebuffers(1, &captureFBO);
        glGenRenderbuffers(1, &captureRBO);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);


        TextureParameters tProps;
        tProps.Format = TextureFormat::RGBA16F;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Faces = tProps.NumArraySlices = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        
        m_Cubemap = Texture::Create(tProps);

        
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        // pbr: convert HDR equirectangular environment map to cubemap equivalent
        // ----------------------------------------------------------------------
        rapi.SetRenderTarget(m_Capture);
        rapi.SetGraphicsPipeline(m_EquirectangularToCubemap);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        rapi.SetViewport(0, 0, 512, 512);
        
        for (unsigned int i = 0; i < 6; ++i)
        {
            memcpy(data, glm::value_ptr(captureProjection), sizeof(glm::mat4));
            memcpy(data + sizeof(glm::mat4), glm::value_ptr(captureViews[i]), sizeof(glm::mat4));
            equirectangularToCubemapShader->SetVSSystemUniformBuffer(data, sizeof(glm::mat4) * 2, 0);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap,
0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            RenderCube();
        }
        rapi.SetRenderTarget(nullptr);

        unsigned int irradianceMap;
        glGenTextures(1, &irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

        // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
        // -----------------------------------------------------------------------------
        irradianceShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

        glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            memcpy(data, glm::value_ptr(captureProjection), sizeof(glm::mat4));
            memcpy(data + sizeof(glm::mat4), glm::value_ptr(captureViews[i]), sizeof(glm::mat4));
            irradianceShader->SetVSSystemUniformBuffer(data, sizeof(glm::mat4) * 2, 0);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
irradianceMap, 0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            RenderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        uint32_t prefilterMap;
        glGenTextures(1, &prefilterMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        for (uint32_t i = 0; i < 6; i++)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        prefilterShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);;
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        uint32_t maxMipLevels = 5;
        for (uint32_t mip = 0; mip < maxMipLevels; mip++)
        {
            uint32_t mipWidth = 128 * std::pow(0.5, mip);
            uint32_t mipHeight = 128 * std::pow(0.5, mip);
            glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
            glViewport(0, 0, mipWidth, mipHeight);
            float roughness = (float)mip / (float)(maxMipLevels - 1);
            prefilterShader->SetFSUserUniformBuffer((byte*)&roughness, sizeof(float));
            for (uint32_t i = 0; i < 6; i++)
            {
                memcpy(data, glm::value_ptr(captureProjection), sizeof(glm::mat4));
                memcpy(data + sizeof(glm::mat4), glm::value_ptr(captureViews[i]), sizeof(glm::mat4));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
prefilterMap, mip); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); RenderCube();
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        uint32_t brdfLUTTexture;
        glGenTextures(1, &brdfLUTTexture);

        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

        glViewport(0, 0, 512, 512);
        brdfShader->Bind();
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_Cubemap = envCubemap;
        m_IrradianceMap = irradianceMap;
        m_BrdfLUTTexture = brdfLUTTexture;
        m_PrefilterMap = prefilterMap;*/
    }

}
