#pragma once

#include "math.h"

struct AABB
{
	Vec3 Min, Max;
};

struct Triangle
{
	union {
		struct {
			Vec3 A, B, C;
		};
		struct {
			Vec3 P[3];
		};
	};
};


enum PlaneRelation
{
	PlaneNegative = -1,
	PlanePositive = 1,
	PlaneIntersect = 0,
};

PlaneRelation CompareAABBvPlane(const Vec3& center, const Vec3& halfSize, const Vec3& normal, float t);
inline bool IntersectAABBvPlane(const Vec3& center, const Vec3& halfSize, const Vec3& normal, float t)
{
	return CompareAABBvPlane(center, halfSize, normal, t) == PlaneIntersect;
}

inline bool IntersectAABBvAABB(const AABB& a, const AABB& b)
{
	if (a.Min.x > b.Max.x || a.Min.y > b.Max.y || a.Min.z > b.Max.z) return false;
	if (b.Min.x > a.Max.x || b.Min.y > a.Max.y || b.Min.z > a.Max.z) return false;
	return true;
}

bool IntersectAABBvTriangle(const Vec3& center, const Vec3& halfSize, const Vec3& a, const Vec3& b, const Vec3& c);
bool IntersectAABBvTriangle(const Vec3& center, const Vec3& halfSize, const Triangle& triangle);
bool IntersectAABBvTriangle(const AABB& aabb, const Triangle& triangle);

