#version 430

layout (local_size_x = 64) in;

struct Particle
{
	vec4 PositionAndLifetime;
	vec4 Velocity;
};

layout (std430, binding=0) buffer Particles
{
	Particle b_Particles[];
};

layout (std430, binding=1) buffer NewParticles
{
	Particle b_NewParticles[];
};

layout (std140, binding=0) uniform Uniform
{
	ivec4 u_Counts;
	ivec4 u_Copy;
	vec4 u_GridBase;
	vec4 u_InvGridSize;
	vec4 u_Dt;
	vec4 u_Gravity;
};

void main()
{
	int id = int(gl_GlobalInvocationID.x);
	if (id < u_Copy.y)
		b_Particles[u_Copy.x + id] = b_NewParticles[id];
}

