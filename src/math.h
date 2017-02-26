#pragma once

#include <math.h>
#include <assert.h>
#include <stdint.h>

#define ASSERT_NEARLY_NORMALIZED(vec) (assert(fabs(length_squared(vec) - 1.0f) < 0.01f))

struct Mat44
{
	union {
		float data[16];
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
	};
};

struct Vec2
{
	float x, y;
};

struct Vec3
{
	float x, y, z;
};

struct Vec4
{
	float x, y, z, w;
};

inline Vec2 vec2(float x, float y)
{
	Vec2 ret;
	ret.x = x;
	ret.y = y;
	return ret;
}

inline Vec2 operator-(const Vec2& a)
{
	Vec2 ret;

	ret.x = -a.x;
	ret.y = -a.y;

	return ret;
}

inline Vec2 operator+(const Vec2& a, const Vec2& b)
{
	Vec2 ret;

	ret.x = a.x + b.x;
	ret.y = a.y + b.y;

	return ret;
}

inline Vec2& operator+=(Vec2& a, const Vec2& b)
{
	a.x += b.x;
	a.y += b.y;
	return a;
}

inline Vec2& operator-=(Vec2& a, const Vec2& b)
{
	a.x -= b.x;
	a.y -= b.y;
	return a;
}

inline Vec2 operator-(const Vec2& a, const Vec2& b)
{
	Vec2 ret;

	ret.x = a.x - b.x;
	ret.y = a.y - b.y;

	return ret;
}

inline Vec3 vec3(float x, float y, float z)
{
	Vec3 ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	return ret;
}

inline Vec3 vec3s(float s)
{
	Vec3 ret;
	ret.x = s;
	ret.y = s;
	ret.z = s;
	return ret;
}

inline Vec3 operator-(const Vec3& a)
{
	Vec3 ret;

	ret.x = -a.x;
	ret.y = -a.y;
	ret.z = -a.z;

	return ret;
}

inline Vec3 operator+(const Vec3& a, const Vec3& b)
{
	Vec3 ret;

	ret.x = a.x + b.x;
	ret.y = a.y + b.y;
	ret.z = a.z + b.z;

	return ret;
}

inline Vec3& operator+=(Vec3& a, const Vec3& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}
inline Vec3& operator-=(Vec3& a, const Vec3& b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}

inline Vec3 operator-(const Vec3& a, const Vec3& b)
{
	Vec3 ret;

	ret.x = a.x - b.x;
	ret.y = a.y - b.y;
	ret.z = a.z - b.z;

	return ret;
}

inline Vec3 operator*(const Vec3& a, const Vec3& b)
{
	Vec3 ret;

	ret.x = a.x * b.x;
	ret.y = a.y * b.y;
	ret.z = a.z * b.z;

	return ret;
}

inline Vec3 operator*(const Vec3& a, float b)
{
	Vec3 ret;

	ret.x = a.x * b;
	ret.y = a.y * b;
	ret.z = a.z * b;

	return ret;
}

inline Vec3 operator*(float a, const Vec3& b)
{
	Vec3 ret;

	ret.x = a * b.x;
	ret.y = a * b.y;
	ret.z = a * b.z;

	return ret;
}

inline float dot(const Vec3& a, const Vec3& b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
	Vec3 ret;

	ret.x = a.y*b.z - a.z*b.y;
	ret.y = a.z*b.x - a.x*b.z;
	ret.z = a.x*b.y - a.y*b.x;

	return ret;
}

inline Vec4 vec4(float x, float y, float z, float w)
{
	Vec4 ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	ret.w = w;
	return ret;
}

inline Vec4 vec4(const Vec3& xyz, float w)
{
	return vec4(xyz.x, xyz.y, xyz.z, w);
}

inline Vec4& operator*=(Vec4& a, float b)
{
	a.x *= b;
	a.y *= b;
	a.z *= b;
	a.w *= b;
	return a;
}

inline float length_squared(const Vec3& a)
{
	return a.x*a.x + a.y*a.y + a.z*a.z;
}

inline float length(const Vec3& a)
{
	return sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
}

inline Vec3 normalize(const Vec3& a)
{
	return a * (1.0f / length(a));
}

static const Mat44 mat44_identity = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f,
};

static const Mat44 mat44_zero = {
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f,
};

const Vec3 vec3_zero = { 0.0f, 0.0f, 0.0f };

inline Mat44 mat44_translate(const Vec3& translation)
{
	Mat44 ret = mat44_identity;

	ret._14 = translation.x;
	ret._24 = translation.y;
	ret._34 = translation.z;

	return ret;
}

inline Mat44 mat44_scale(const Vec3& scale)
{
	Mat44 ret = mat44_identity;

	ret._11 = scale.x;
	ret._22 = scale.y;
	ret._33 = scale.z;

	return ret;
}

inline Mat44 mat44_rotate_x(float angle)
{
	Mat44 ret = mat44_identity;

	float cos = cosf(angle);
	float sin = sinf(angle);

	ret._22 = cos;
	ret._23 = -sin;
	ret._32 = sin;
	ret._33 = cos;

	return ret;
}

inline Mat44 mat44_rotate_y(float angle)
{
	Mat44 ret = mat44_identity;

	float cos = cosf(angle);
	float sin = sinf(angle);

	ret._11 = cos;
	ret._13 = sin;
	ret._31 = -sin;
	ret._33 = cos;

	return ret;
}

inline Mat44 mat44_rotate_z(float angle)
{
	Mat44 ret = mat44_identity;

	float cos = cosf(angle);
	float sin = sinf(angle);

	ret._11 = cos;
	ret._12 = -sin;
	ret._21 = sin;
	ret._22 = cos;

	return ret;
}

inline Mat44 mat44_rotate_normalized_axis(const Vec3& axis, float angle)
{
	Mat44 ret;

	ASSERT_NEARLY_NORMALIZED(axis);

	float cos = cosf(angle);
	float mcos = 1.0f - cos;
	float sin = sinf(angle);

	ret._11 = cos + axis.x*axis.x*mcos;
	ret._12 = axis.x*axis.y*mcos - axis.z*sin;
	ret._13 = axis.x*axis.z*mcos + axis.y*sin;
	ret._14 = 0.0f;

	ret._21 = axis.x*axis.y*mcos + axis.z*sin;
	ret._22 = cos + axis.y*axis.y*mcos;
	ret._23 = axis.y*axis.z*mcos - axis.x*sin;
	ret._24 = 0.0f;

	ret._31 = axis.x*axis.z*mcos - axis.y*sin;
	ret._32 = axis.y*axis.z*mcos + axis.x*sin;
	ret._33 = cos + axis.z*axis.z*mcos;
	ret._34 = 0.0f;

	ret._41 = 0.0f;
	ret._42 = 0.0f;
	ret._43 = 0.0f;
	ret._44 = 1.0f;

	return ret;
}

inline Mat44 mat44_rotate_axis(const Vec3& axis, float angle)
{
	return mat44_rotate_normalized_axis(normalize(axis), angle);
}

inline Mat44 mat44_axes(const Vec3& forward, const Vec3& right, const Vec3& up, const Vec3& pos = vec3_zero)
{
	Mat44 ret;

	ret._11 = right.x;
	ret._12 = right.y;
	ret._13 = right.z;
	ret._14 = -dot(pos, right);

	ret._21 = up.x;
	ret._22 = up.y;
	ret._23 = up.z;
	ret._24 = -dot(pos, up);

	ret._31 = -forward.x;
	ret._32 = -forward.y;
	ret._33 = -forward.z;
	ret._34 = dot(pos, forward);

	ret._41 = 0.0f;
	ret._42 = 0.0f;
	ret._43 = 0.0f;
	ret._44 = 1.0f;

	return ret;
}

inline Mat44 mat44_lookat(const Vec3& eye, const Vec3& target, const Vec3& up)
{
	Vec3 forward = normalize(target - eye);
	Vec3 right = normalize(cross(forward, up));
	Vec3 new_up = cross(right, forward);

	return mat44_axes(forward, right, new_up, eye);
}

inline Mat44 mat44_perspective(float fov, float aspect, float near_z, float far_z)
{
	Mat44 ret = mat44_zero;

	float tan = tanf(fov / 2.0f);
	float len = far_z - near_z;

	ret._11 = 1.0f / (aspect * tan);
	ret._22 = 1.0f / (tan);
	ret._33 = -(far_z + near_z) / len;
	ret._34 = -(2 * far_z * near_z) / len;
	ret._43 = -1.0f;

	return ret;
}

inline Mat44 operator*(const Mat44& a, const Mat44& b)
{
	Mat44 ret;

	ret._11 = a._11*b._11 + a._21*b._12 + a._31*b._13 + a._41*b._14;
	ret._12 = a._12*b._11 + a._22*b._12 + a._32*b._13 + a._42*b._14;
	ret._13 = a._13*b._11 + a._23*b._12 + a._33*b._13 + a._43*b._14;
	ret._14 = a._14*b._11 + a._24*b._12 + a._34*b._13 + a._44*b._14;

	ret._21 = a._11*b._21 + a._21*b._22 + a._31*b._23 + a._41*b._24;
	ret._22 = a._12*b._21 + a._22*b._22 + a._32*b._23 + a._42*b._24;
	ret._23 = a._13*b._21 + a._23*b._22 + a._33*b._23 + a._43*b._24;
	ret._24 = a._14*b._21 + a._24*b._22 + a._34*b._23 + a._44*b._24;

	ret._31 = a._11*b._31 + a._21*b._32 + a._31*b._33 + a._41*b._34;
	ret._32 = a._12*b._31 + a._22*b._32 + a._32*b._33 + a._42*b._34;
	ret._33 = a._13*b._31 + a._23*b._32 + a._33*b._33 + a._43*b._34;
	ret._34 = a._14*b._31 + a._24*b._32 + a._34*b._33 + a._44*b._34;

	ret._41 = a._11*b._41 + a._21*b._42 + a._31*b._43 + a._41*b._44;
	ret._42 = a._12*b._41 + a._22*b._42 + a._32*b._43 + a._42*b._44;
	ret._43 = a._13*b._41 + a._23*b._42 + a._33*b._43 + a._43*b._44;
	ret._44 = a._14*b._41 + a._24*b._42 + a._34*b._43 + a._44*b._44;

	return ret;
}

inline Mat44 transpose(const Mat44& a)
{
	Mat44 ret;

	ret._11 = a._11;
	ret._12 = a._21;
	ret._13 = a._31;
	ret._14 = a._41;

	ret._21 = a._12;
	ret._22 = a._22;
	ret._23 = a._32;
	ret._24 = a._42;

	ret._31 = a._13;
	ret._32 = a._23;
	ret._33 = a._33;
	ret._34 = a._43;

	ret._41 = a._14;
	ret._42 = a._24;
	ret._43 = a._34;
	ret._44 = a._44;

	return ret;
}

inline float determinant(const Mat44& a)
{
	return 
		+ a._11*a._22*a._33*a._44 + a._11*a._23*a._34*a._42 + a._11*a._24*a._32*a._43
		+ a._12*a._21*a._34*a._43 + a._12*a._23*a._31*a._44 + a._12*a._24*a._33*a._41
		+ a._13*a._21*a._32*a._44 + a._13*a._22*a._34*a._41 + a._13*a._24*a._31*a._42
		+ a._14*a._21*a._33*a._42 + a._14*a._22*a._31*a._43 + a._14*a._23*a._32*a._41
		- a._11*a._22*a._34*a._43 - a._11*a._23*a._32*a._44 - a._11*a._24*a._33*a._42
		- a._12*a._21*a._33*a._44 - a._11*a._23*a._34*a._41 - a._12*a._24*a._31*a._43
		- a._13*a._21*a._34*a._42 - a._13*a._22*a._31*a._44 - a._13*a._24*a._32*a._41
		- a._14*a._21*a._32*a._43 - a._14*a._22*a._33*a._41 - a._14*a._23*a._31*a._42;
}

inline Mat44 inverse(const Mat44& mat)
{
	Mat44 ret;

#define MATH_MAT44INVERSE_DET3(a,b,c,d,e,f,g,h,i) \
	(mat._##a*mat._##e*mat._##i + mat._##b*mat._##f*mat._##g + mat._##c*mat._##d*mat._##h \
	 - mat._##a*mat._##f*mat._##h - mat._##b*mat._##d*mat._##i - mat._##c*mat._##e*mat._##g)

	ret._11 = + MATH_MAT44INVERSE_DET3(22, 23, 24, 32, 33, 34, 42, 43, 44);
	ret._21 = - MATH_MAT44INVERSE_DET3(23, 24, 21, 33, 34, 31, 43, 44, 41);
	ret._31 = + MATH_MAT44INVERSE_DET3(24, 21, 22, 34, 31, 32, 44, 41, 42);
	ret._41 = - MATH_MAT44INVERSE_DET3(21, 22, 23, 31, 32, 33, 41, 42, 43);

	ret._12 = - MATH_MAT44INVERSE_DET3(32, 33, 34, 42, 43, 44, 12, 13, 14);
	ret._22 = + MATH_MAT44INVERSE_DET3(33, 34, 31, 43, 44, 41, 13, 14, 11);
	ret._32 = - MATH_MAT44INVERSE_DET3(34, 31, 32, 44, 41, 42, 14, 11, 12);
	ret._42 = + MATH_MAT44INVERSE_DET3(31, 32, 33, 41, 42, 43, 11, 12, 13);

	ret._13 = + MATH_MAT44INVERSE_DET3(42, 43, 44, 12, 13, 14, 22, 23, 24);
	ret._23 = - MATH_MAT44INVERSE_DET3(43, 44, 41, 13, 14, 11, 23, 24, 21);
	ret._33 = + MATH_MAT44INVERSE_DET3(44, 41, 42, 14, 11, 12, 24, 21, 22);
	ret._43 = - MATH_MAT44INVERSE_DET3(41, 42, 43, 11, 12, 13, 21, 22, 23);

	ret._14 = - MATH_MAT44INVERSE_DET3(12, 13, 14, 22, 23, 24, 32, 33, 34);
	ret._24 = + MATH_MAT44INVERSE_DET3(13, 14, 11, 23, 24, 21, 33, 34, 31);
	ret._34 = - MATH_MAT44INVERSE_DET3(14, 11, 12, 24, 21, 22, 34, 31, 32);
	ret._44 = + MATH_MAT44INVERSE_DET3(11, 12, 13, 21, 22, 23, 31, 32, 33);

#undef MATH_MAT44INVERSE_DET3

	float det = mat._11*ret._11 + mat._12*ret._21 + mat._13*ret._31 + mat._14*ret._41;
	assert(fabs(det) > 0.0f);

	float idet = 1.0f / det;

	ret._11 *= idet; ret._12 *= idet; ret._13 *= idet; ret._14 *= idet;
	ret._21 *= idet; ret._22 *= idet; ret._23 *= idet; ret._24 *= idet;
	ret._31 *= idet; ret._32 *= idet; ret._33 *= idet; ret._34 *= idet;
	ret._41 *= idet; ret._42 *= idet; ret._43 *= idet; ret._44 *= idet;

	return ret;
}

inline Mat44& operator*=(Mat44& a, const Mat44& b)
{
	a = a * b;
	return a;
}

inline Vec3 operator*(const Vec3& vec, const Mat44& mat)
{
	Vec3 ret;

	ret.x = vec.x*mat._11 + vec.y*mat._12 + vec.z*mat._13 + mat._14;
	ret.y = vec.x*mat._21 + vec.y*mat._22 + vec.z*mat._23 + mat._24;
	ret.z = vec.x*mat._31 + vec.y*mat._32 + vec.z*mat._33 + mat._34;

	return ret;
}

inline Vec4 operator*(const Vec4& vec, const Mat44& mat)
{
	Vec4 ret;

	ret.x = vec.x*mat._11 + vec.y*mat._12 + vec.z*mat._13 + mat._14;
	ret.y = vec.x*mat._21 + vec.y*mat._22 + vec.z*mat._23 + mat._24;
	ret.z = vec.x*mat._31 + vec.y*mat._32 + vec.z*mat._33 + mat._34;
	ret.w = vec.x*mat._41 + vec.y*mat._42 + vec.z*mat._43 + mat._44;

	return ret;
}
