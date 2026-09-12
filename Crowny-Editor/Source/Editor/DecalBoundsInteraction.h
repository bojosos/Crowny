#pragma once
#include "Editor/UndoRedo.h"
#include <array>

namespace Crowny
{
    class DecalBoundsInteraction
    {
    public:
        struct Handle
        {
            glm::vec3 Position;
            glm::vec3 Direction;
            const char* Label;
        };
        ~DecalBoundsInteraction();
        bool Draw(Entity target, const glm::mat4& viewProjection, const glm::vec4& viewport, bool editable, float snap, float angularSnap = 0);
        bool Begin(Entity target, int handle);
        void Update(float delta, bool symmetric, float snap = 0);
        void Commit();
        void Cancel();
        bool IsUsing() const { return m_Handle >= 0; }
        static DecalSettings Resize(const DecalSettings& before, int handle, float delta, bool symmetric);
        static glm::vec3 CylinderPoint(const DecalSettings& settings, float angle, float heightFraction, float shellOffset = 0);
        static std::array<Handle, 7> CylinderHandles(const DecalSettings& settings);

    private:
        void Finish(bool cancel);
        Ref<Scene> m_Scene;
        UUID m_Target;
        DecalSettings m_Before;
        glm::vec2 m_MouseStart{ 0 }, m_Direction{ 0 };
        int m_Handle = -1;
    };
} // namespace Crowny
