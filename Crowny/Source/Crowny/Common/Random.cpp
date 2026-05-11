#include "cwpch.h"

#include "Crowny/Common/Random.h"

#include <glm/gtc/constants.hpp>

namespace Crowny
{
    void Random::OnStartUp() { m_RandomEngine.seed(std::random_device()()); }

    void Random::Seed(uint32_t seed) { Get().m_RandomEngine.seed(seed ? seed : std::random_device()()); }

    float Random::Float() { return (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max(); }

    float Random::Float(float min, float max)
    {
        const float randf = (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
        return min + (max - min) * randf;
    }

    int32_t Random::Int(int32_t min, int32_t max)
    {
        const float randf = (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
        return (int32_t)(min + (max - min) * randf);
    }

    glm::vec3 Random::InsideUnitSphere()
    {
        const float theta = Random::Float(0, glm::two_pi<float>());
        const float v = Random::Float();
        const float phi = glm::acos((2 * v) - 1);
        const float r = glm::pow(Random::Float(), 1.0f / 3.0f);
        const float x = r * glm::sin(phi) * glm::cos(theta);
        const float y = r * glm::sin(phi) * glm::sin(theta);
        const float z = r * glm::cos(phi);
        return glm::vec3(x, y, z);
    }

    glm::vec2 Random::InsideUnitCircle()
    {
        const float a = Random::Float() * 2 * glm::pi<float>();
        const float r = glm::sqrt(Random::Float());
        return glm::vec2(r * glm::cos(a), r * glm::sin(a));
    }

} // namespace Crowny