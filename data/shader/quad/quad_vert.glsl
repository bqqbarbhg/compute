#version 430

layout (std140, binding=0) uniform Block
{
	mat4 u_Projection;
};

layout (location=0) in vec4 in_Position;
layout (location=1) in vec2 in_TexCoord;
layout (location=2) in float in_Fade;

out VertexData
{
	vec2 v_TexCoord;
	float v_Fade;
};

void main()
{
	gl_Position = in_Position.xyzz;
	v_TexCoord = in_TexCoord;
	v_Fade = in_Fade;
}

