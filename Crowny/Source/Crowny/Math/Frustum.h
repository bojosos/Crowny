#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
	class ViewFrustum
	{
		struct Plane {
			float DistanceToPoint(const glm::vec3& point) const;

			float distanceToOrigin;
			glm::vec3 normal;
		};

	public:
		void Update(const glm::mat4& projViewMatrix);

		bool ChunkIsInFrustum(glm::vec3 position) const;

	private:
		std::array<Plane, 6> m_Planes;
	};
}