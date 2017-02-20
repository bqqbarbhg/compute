#version 430

layout (local_size_x = 64) in;

struct Triangle
{
	vec4 A, B, C;
};

struct Particle
{
	vec4 PositionAndLifetime;
	vec4 Velocity;
};

layout (std140, binding=0) uniform Uniform
{
	ivec4 u_Counts;
	vec4 u_Dt;
	vec4 u_Gravity;
};

layout (std430, binding=0) buffer Particles
{
	Particle b_Particles[];
};

layout (std430, binding=1) buffer Triangles
{
	Triangle b_Triangles[];
};

bool IntersectRayTriangle(vec3 ro, vec3 rd, vec3 a, vec3 b, vec3 c, out float outT)
{
	vec3 e0 = b - a;
	vec3 e1 = c - a;

	vec3 p = cross(rd, e1);
	float det = dot(e0, p);

	if (abs(det) < 0.0001)
		return false;

	float invDet = 1.0 / det;

	vec3 t = ro - a;
	float u = dot(t, p) * invDet;

	vec3 q = cross(t, e0);
	float v = dot(rd, q) * invDet;

	if (min(u, min(v, 1.0 - u - v)) < 0.0)
		return false;

	outT = dot(e1, q) * invDet;
	return true;
}

void main()
{
	int id = int(gl_GlobalInvocationID.x);

	Particle particle = b_Particles[id];

	float life = particle.PositionAndLifetime.w - u_Dt.x;
	vec3 pos = particle.PositionAndLifetime.xyz;
	vec3 newPos = pos + particle.Velocity.xyz * u_Dt.x;
	vec3 newVel = particle.Velocity.xyz + u_Gravity.xyz;

	vec3 delta = newPos - pos;
	vec3 dir = normalize(delta);
	float dlen = length(delta);

	for (int i = 0; i < u_Counts.x; i++)
	{
		Triangle t = b_Triangles[i];

		float tt;
		if (IntersectRayTriangle(pos, dir, t.A.xyz, t.B.xyz, t.C.xyz, tt) && tt >= 0.0 && tt < dlen)
		{
			vec3 n = normalize(cross(t.B.xyz - t.A.xyz, t.C.xyz - t.A.xyz));
			newPos = pos;
			newVel = (newVel - n * 2.0 * dot(newVel, n)) * 0.7;
			break;
		}
	}

	b_Particles[id].PositionAndLifetime = vec4(newPos, life);
	b_Particles[id].Velocity = vec4(newVel, 0.0);
}

