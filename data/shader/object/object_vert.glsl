#version 430

layout (std140, binding=0) uniform Block
{
	mat4 u_ModelViewProjection;
};

layout (location=0) in vec4 in_Position;
layout (location=1) in vec2 in_TexCoord;

out VertexData
{
	vec2 v_TexCoord;
};

void main()
{
	gl_Position = u_ModelViewProjection * in_Position;
	v_TexCoord = in_TexCoord;
}

