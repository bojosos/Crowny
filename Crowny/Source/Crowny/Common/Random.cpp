#include "cwpch.h"

#include "Crowny/Common/Random.h"

#include <glm/gtc/constants.hpp>

namespace Crowny
{
    void Random::OnStartUp() { m_RandomEngine.seed(std::random_device()()); }

    void Random::Seed(uint32_t seed) { Get().m_RandomEngine.seed(seed ? seed : std::random_device()()); }

    float Random::Float()
    {
        return (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
    }

    float Random::Float(float min, float max)
    {
        float randf =
          (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
        return min + (max - min) * randf;
    }

    int32_t Random::Int(int32_t min, int32_t max)
    {
        float randf =
          (float)(Get().m_Distribution(Get().m_RandomEngine)) / (float)std::numeric_limits<RandomValueType>::max();
        return (int32_t)(min + (max - min) * randf);
    }

    glm::vec3 Random::InsideUnitSphere()
    {
        float theta = Random::Float(0, glm::two_pi<float>());
        float v = Random::Float();
        float phi = glm::acos((2 * v) - 1);
        float r = glm::pow(Random::Float(), 1.0f / 3.0f);
        float x = r * glm::sin(phi) * glm::cos(theta);
        float y = r * glm::sin(phi) * glm::sin(theta);
        float z = r * glm::cos(phi);
        return glm::vec3(x, y, z);
    }

    glm::vec2 Random::InsideUnitCircle()
    {
        float a = Random::Float() * 2 * glm::pi<float>();
        float r = glm::sqrt(Random::Float());
        return glm::vec2(r * glm::cos(a), r * glm::sin(a));
    }

} // namespace Crowny