#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics2D.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Physics/Physics2D.h"

#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_body.h>

#include <mono/metadata/object.h>

namespace Crowny
{

    struct RaycastHit2DInterop
    {
        glm::vec2 Point;
        glm::vec2 Normal;
        float Fraction;
        uint32_t EntityId;
    };

    class RaycastCallback : public b2RayCastCallback
    {
    public:
        RaycastCallback(uint32_t layerMask) : m_LayerMask(layerMask) {}

        float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override
        {
            if (m_LayerMask != 0xFFFFFFFF)
            {
                uint16_t categoryBits = fixture->GetFilterData().categoryBits;
                if ((categoryBits & m_LayerMask) == 0)
                    return -1.0f;
            }

            RaycastHit2DInterop hit;
            hit.Point = { point.x, point.y };
            hit.Normal = { normal.x, normal.y };
            hit.Fraction = fraction;
            hit.EntityId = (uint32_t)fixture->GetBody()->GetUserData().pointer;
            m_Hits.push_back(hit);

            return 1.0f; // Continue to find all hits
        }

        Vector<RaycastHit2DInterop> m_Hits;

    private:
        uint32_t m_LayerMask;
    };

    void ScriptPhysics2D::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Raycast", (void*)&Internal_Raycast);
    }

    void ScriptPhysics2D::Internal_Raycast(glm::vec2* origin, glm::vec2* direction, float distance, uint32_t layerMask, MonoArray** outResults)
    {
        *outResults = nullptr;

        b2World* world = Physics2D::Get().GetPhysicsWorld();
        if (!world)
            return;

        b2Vec2 start(origin->x, origin->y);
        glm::vec2 dir = glm::normalize(*direction);
        b2Vec2 end(origin->x + dir.x * distance, origin->y + dir.y * distance);

        RaycastCallback callback(layerMask);
        world->RayCast(&callback, start, end);

        if (callback.m_Hits.empty())
            return;

        std::sort(callback.m_Hits.begin(), callback.m_Hits.end(),
            [](const RaycastHit2DInterop& a, const RaycastHit2DInterop& b) { return a.Fraction < b.Fraction; });

        MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        MonoClass* hitClass = assembly ? assembly->GetClass("Crowny", "RaycastHit2D") : nullptr;
        ::MonoClass* rawClass = hitClass ? hitClass->GetInternalPtr() : MonoUtils::GetI32Class();

        *outResults = mono_array_new(MonoManager::Get().GetDomain(), rawClass, (uintptr_t)callback.m_Hits.size());

        for (size_t i = 0; i < callback.m_Hits.size(); i++)
            mono_array_set(*outResults, RaycastHit2DInterop, i, callback.m_Hits[i]);
    }

} // namespace Crowny
