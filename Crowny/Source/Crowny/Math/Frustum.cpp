#include "cwpch.h"
#include "Frustum.h"

namespace Crowny
{

	enum Planes {
		Near,
		Far,
		Left,
		Right,
		Top,
		Bottom,
	};

	float ViewFrustum::Plane::DistanceToPoint(const glm::vec3& point) const
	{
		return glm::dot(point, normal) + distanceToOrigin;
	}

	void ViewFrustum::Update(const glm::mat4& mat)
	{
		// Left
		m_Planes[Planes::Left].normal.x = mat[0][3] + mat[0][0];
		m_Planes[Planes::Left].normal.y = mat[1][3] + mat[1][0];
		m_Planes[Planes::Left].normal.z = mat[2][3] + mat[2][0];
		m_Planes[Planes::Left].distanceToOrigin = mat[3][3] + mat[3][0];

		// Right
		m_Planes[Planes::Right].normal.x = mat[0][3] - mat[0][0];
		m_Planes[Planes::Right].normal.y = mat[1][3] - mat[1][0];
		m_Planes[Planes::Right].normal.z = mat[2][3] - mat[2][0];
		m_Planes[Planes::Right].distanceToOrigin = mat[3][3] - mat[3][0];

		// Bottom
		m_Planes[Planes::Bottom].normal.x = mat[0][3] + mat[0][1];
		m_Planes[Planes::Bottom].normal.y = mat[1][3] + mat[1][1];
		m_Planes[Planes::Bottom].normal.z = mat[2][3] + mat[2][1];
		m_Planes[Planes::Bottom].distanceToOrigin = mat[3][3] + mat[3][1];

		// Top
		m_Planes[Planes::Top].normal.x = mat[0][3] - mat[0][1];
		m_Planes[Planes::Top].normal.y = mat[1][3] - mat[1][1];
		m_Planes[Planes::Top].normal.z = mat[2][3] - mat[2][1];
		m_Planes[Planes::Top].distanceToOrigin = mat[3][3] - mat[3][1];

		// Near
		m_Planes[Planes::Near].normal.x = mat[0][3] + mat[0][2];
		m_Planes[Planes::Near].normal.y = mat[1][3] + mat[1][2];
		m_Planes[Planes::Near].normal.z = mat[2][3] + mat[2][2];
		m_Planes[Planes::Near].distanceToOrigin = mat[3][3] + mat[3][2];

		// Far
		m_Planes[Planes::Far].normal.x = mat[0][3] - mat[0][2];
		m_Planes[Planes::Far].normal.y = mat[1][3] - mat[1][2];
		m_Planes[Planes::Far].normal.z = mat[2][3] - mat[2][2];
		m_Planes[Planes::Far].distanceToOrigin = mat[3][3] - mat[3][2];

		for (auto& plane : m_Planes) {
			float length = glm::length(plane.normal);
			plane.normal /= length;
			plane.distanceToOrigin /= length;
		}
	}

	/*
	bool ViewFrustum::ChunkIsInFrustum(glm::vec3 box) const
	{
		box *= CHUNK_SIZE;

		auto getVP = [&](const glm::vec3& normal) {
			auto res = box;

			if (normal.x > 0) {
				res.x += CHUNK_SIZE;
			}
			if (normal.y > 0) {
				res.y += CHUNK_SIZE;
			}
			if (normal.z > 0) {
				res.z += CHUNK_SIZE;
			}

			return glm::vec3{ res.x, res.y, res.z };
		};

		auto getVN = [&](const glm::vec3& normal) {
			auto res = box;

			if (normal.x < 0) {
				res.x += CHUNK_SIZE;
			}
			if (normal.y < 0) {
				res.y += CHUNK_SIZE;
			}
			if (normal.z < 0) {
				res.z += CHUNK_SIZE;
			}

			return glm::vec3{ res.x, res.y, res.z };
		};

		bool result = true;
		for (auto& plane : m_Planes) {
			if (plane.DistanceToPoint(getVP(plane.normal)) < 0) {
				return false;
			}
			else if (plane.DistanceToPoint(getVN(plane.normal)) < 0) {
				result = true;
			}
		}
		return result;
	}*/
}