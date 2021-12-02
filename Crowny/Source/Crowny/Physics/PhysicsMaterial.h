#pragma once

namespace Crowny
{
    struct PhysicsMaterial2D
    {
        float Density = 1.0f;
        float Friction = 0.5f;
        float Restitution = 0.0f;
        float RestitutionThreshold = 0.5f;
    };
} // namespace Crowny