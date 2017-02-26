#include "intersection.h"
#include "fastmath.h"

PlaneRelation CompareAABBvPlane(const Vec3& center, const Vec3& halfSize, const Vec3& normal, float t)
{
	float mx, my, mz;
	mx = F_COPYSIGN_TO_POSITIVE(halfSize.x, normal.x);
	my = F_COPYSIGN_TO_POSITIVE(halfSize.y, normal.y);
	mz = F_COPYSIGN_TO_POSITIVE(halfSize.z, normal.z);

	float mdot = mx * normal.x + my * normal.y + mz * normal.z;
	float cdot = center.x * normal.x + center.y * normal.y + center.z * normal.z;

	if (cdot - mdot > t) return PlanePositive;
	if (cdot + mdot < t) return PlaneNegative;
	return PlaneIntersect;
}

inline Vec3 absVec(Vec3 v)
{
	return vec3(F_ABS(v.x), F_ABS(v.y), F_ABS(v.z));
}

bool IntersectAABBvTriangle(const Vec3& center, const Vec3& halfSize, const Vec3& a, const Vec3& b, const Vec3& c)
{
	Vec3 ao = a - center, bo = b - center, co = c - center;

	// Axis aligned separating planes
	if (F_MIN3(ao.x, bo.x, co.x) > halfSize.x) return false;
	if (F_MIN3(ao.y, bo.y, co.y) > halfSize.y) return false;
	if (F_MIN3(ao.z, bo.z, co.z) > halfSize.z) return false;
	if (F_MAX3(ao.x, bo.x, co.x) < -halfSize.x) return false;
	if (F_MAX3(ao.y, bo.y, co.y) < -halfSize.y) return false;
	if (F_MAX3(ao.z, bo.z, co.z) < -halfSize.z) return false;

	Vec3 e0 = b - a, e1 = c - b, e2 = a - c;

	// Triangle plane
	{
		Vec3 normal = cross(e0, e1);
		float t = dot(normal, a);
		if (!IntersectAABBvPlane(center, halfSize, normal, t))
			return false;
	}

	// Cross axes

	// --- SLOW
	Vec3 axes[] = { vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f) };
	Vec3 edges[] = { e0, e1, e2 };

	for (uint32_t axisI = 0; axisI < 3; axisI++)
	{
		Vec3 axis = axes[axisI];
		for (uint32_t edgeI = 0; edgeI < 3; edgeI++)
		{
			Vec3 edge = edges[edgeI];

			Vec3 separatingAxis = cross(axis, edge);
			float radius = dot(absVec(separatingAxis), halfSize);

			float pa = dot(separatingAxis, a);
			float pb = dot(separatingAxis, b);
			float pc = dot(separatingAxis, c);

			if (F_MAX3(pa, pb, pc) < -radius || F_MIN3(pa, pb, pc) > radius)
				return false;
		}
	}

	return true;
}

bool IntersectAABBvTriangle(const Vec3& center, const Vec3& halfSize, const Triangle& triangle)
{
	return IntersectAABBvTriangle(center, halfSize, triangle.A, triangle.B, triangle.C);
}

bool IntersectAABBvTriangle(const AABB& aabb, const Triangle& triangle)
{
	return IntersectAABBvTriangle((aabb.Min + aabb.Max) * 0.5f, (aabb.Max - aabb.Min) * 0.5f, triangle);
}

