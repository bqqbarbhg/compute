#version 430

struct Particle
{
	vec4 PositionAndLifetime;
	vec4 Velocity;
};

layout (std140, binding=0) uniform Block
{
	mat4 u_ModelViewProjection;
};

layout (std430, binding=0) buffer Particles
{
	Particle s_Particles[];
};

layout (location=0) in vec2 in_TexCoord;

out VertexData
{
	vec2 v_TexCoord;
};

void main()
{
	vec4 pos = u_ModelViewProjection * vec4(s_Particles[gl_InstanceID].PositionAndLifetime.xyz, 1.0);
	pos.xy += in_TexCoord.xy * 0.1;
	gl_Position = pos;

	v_TexCoord = in_TexCoord;
}

