#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Physics/PhysicsMaterial.h"

#include <glm/glm.hpp>

namespace Crowny
{
    // These are the ones exposed from Box2D by default. It looks like it wouldn't be hard to expose some more of their
    // #define-s
    struct Physics2DSettings
    {
        PhysicsMaterial2D DefaultMaterial;
        glm::vec2 Gravity = { 0.0f, -9.81f };
        uint32_t VelocityIterations = 8;
        uint32_t PositionIterations = 3;
    };

    class Physics2D : public Module<Physics2D>
    {
    public:
        Physics2D();

        const Ref<Physics2DSettings>& GetSettings() const { return m_Settings; }
        void SetCategoryMask(uint32_t idx, uint32_t mask) { m_MaskBits[idx] = mask; }
        uint32_t GetCategoryMask(uint32_t idx) { return m_MaskBits[idx]; }

    private:
        Ref<Physics2DSettings> m_Settings;
        Array<uint32_t, 32> m_MaskBits;
    };
} // namespace Crowny