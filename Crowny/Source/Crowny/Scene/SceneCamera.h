#pragma once

#include "Crowny/Renderer/Camera.h"

namespace Crowny
{
    
    class SceneCamera : public Camera
	{
    public:
        enum class CameraProjection
	    {
		    Orthographic = 0, Perspective = 1
	    };

	public:
		SceneCamera();
		SceneCamera(const glm::mat4& projection);
		~SceneCamera() = default;

		void SetPerspective(float verticalFov, float near, float far);
        void SetOrthographic(float size, float near, float far);

        void SetViewportSize(uint32_t width, uint32_t height);

        float GetPerspectiveVerticalFOV() const { return m_PerspectiveFOV; }
        void SetPerspectiveVerticalFOV(float fov) { m_PerspectiveFOV = fov; RecalculateProjection(); }
        float GetPerspectiveNearClip() const { return m_PerspectiveNear; }
        void SetPerspectiveNearClip(float near) { m_PerspectiveNear = near; RecalculateProjection(); }
        float GetPerspectiveFarClip() const { return m_PerspectiveFar; }
        void SetPerspectiveFarClip(float far) { m_PerspectiveFar = far; RecalculateProjection(); }

        float GetOrthographicSize() const { return m_OrthographicSize; }
        void SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjection(); }
        float GetOrthographicNearClip() const { return m_OrthographicNear; }
        void SetOrthographicNearClip(float near) { m_OrthographicNear = near; RecalculateProjection(); }
        float GetOrthographicFarClip() const { return m_OrthographicFar; }
        void SetOrthographicFarClip(float far) { m_OrthographicFar = far; RecalculateProjection(); }

        // TODO: Should use glViewport to render
        const glm::vec4& GetViewportRect() const { return m_ViewportRectangle; }
        void SetViewportRect(const glm::vec4& rect) { m_ViewportRectangle = rect; }

        const glm::vec3& GetBackgroundColor() const { return m_BackgroundColor; }
        void SetBackgroundColor(const glm::vec3& color) { m_BackgroundColor = color; }

        CameraProjection GetProjectionType() const { return m_ProjectionType; }
        void SetProjectionType(CameraProjection type) { m_ProjectionType = type; RecalculateProjection(); }

        bool GetHDR() const { return m_HDR; }
        void SetHDR(bool hdr) { m_HDR = hdr; }

        bool GetMSAA() const { return m_MSAA; }
        void SetMSAA(bool value) { m_MSAA = value; }

        bool GetOcclusionCulling() const { return m_OcclusionCulling; }
        void SetOcclusionCulling(bool value) { m_OcclusionCulling = value; }

    private:
        void RecalculateProjection();

	private:
		CameraProjection m_ProjectionType = CameraProjection::Orthographic;
        float m_PerspectiveFOV = glm::radians(45.0f);
        float m_PerspectiveNear = 0.01f, m_PerspectiveFar = 1000.0f;

        float m_OrthographicSize = 10.0f;
        float m_OrthographicNear = -1.0f, m_OrthographicFar = 1.0f;
        float m_AspectRatio = 0.0f;

        glm::vec3 m_BackgroundColor = { 0.0f, 0.3f, 0.3f };
		glm::vec4 m_ViewportRectangle = { 0.0f, 0.0f, 1.0f, 1.0f };
		bool m_HDR = false;
		bool m_MSAA = false;
		bool m_OcclusionCulling = false;
	};
}