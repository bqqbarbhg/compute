#version 430

layout (binding=0) uniform Block
{
	mat4 u_ModelViewProjection;
};

layout (location=0) in vec4 in_Position;
layout (location=1) in vec4 in_Color;

out VertexData
{
	vec4 v_Color;
	vec2 v_TexCoord;
};

void main()
{
	gl_Position = in_Position * u_ModelViewProjection;
	v_Color = in_Color;
	v_TexCoord = in_Position.xy * 8.0;
}

