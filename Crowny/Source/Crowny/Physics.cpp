#include "cwpch.h"

#include "Crowny/Physics.h"
#include "Crowny/Common/Math.h"

namespace Crowny
{
	void Physics::VoxelRaycast(const glm::vec3& location, const glm::vec3& dir, float length, bool button)
	{
		float x = floor(location.x);
		float y = floor(location.y);
		float z = floor(location.z);

		float dx = dir.x;
		float dy = dir.y;
		float dz = dir.z;

		float stepX = Math::Signum(dx);
		float stepY = Math::Signum(dy);
		float stepZ = Math::Signum(dz);

		float tMaxX = Math::Intbound(location.x, dx);
		float tMaxY = Math::Intbound(location.y, dy);
		float tMaxZ = Math::Intbound(location.z, dz);

		float tDeltaX = stepX / dx;
		float tDeltaY = stepY / dy;
		float tDeltaZ = stepZ / dz;

		length /= sqrt(dx * dx + dy * dy + dz * dz);

		glm::vec3 face(0.0f);
		CW_ENGINE_ASSERT(dx != 0.0f || dy != 0.0f || dz != 0.0f, "Raycast has no direction!");

		while ((stepX > 0.0f ? x < 160.0f : x >= 0.0f) && (stepY > 0.0f ? y < 160.0f : y >= 0.0f) && (stepZ > 0.0f ? z < 160.0f : z >= 0.0f)) {
			
			if (!(x < 0.0f || y < 0.0f || z < 0.0f || x >= 160.0f || y >= 160.0f || z >= 160.0f))
				//TODO  Here I have hit something

			if (tMaxX < tMaxY)
			{
				if (tMaxX < tMaxZ)
				{
					if (tMaxX > length) break;
					x += stepX;
					tMaxX += tDeltaX;

					face.x = -stepX;
					face.y = 0;
					face.z = 0;
				}
				else
				{
					if (tMaxZ > length) break;
					z += stepZ;
					tMaxZ += tDeltaZ;

					face.x = 0;
					face.y = 0;
					face.z = -stepZ;;
				}
			}
			else
			{
				if (tMaxY < tMaxZ)
				{
					if (tMaxY > length) break;
					y += stepY;
					tMaxY += tDeltaY;

					face.x = 0;
					face.y = -stepY;
					face.z = 0;
				}
				else
				{
					if (tMaxZ > length) break;
					z += stepZ;
					tMaxZ += tDeltaZ;

					face.x = 0;
					face.y = 0;
					face.z = -stepZ;
				}
			}
		}
	}
}