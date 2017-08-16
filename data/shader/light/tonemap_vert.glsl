#version 430

layout (location = 0) in vec2 in_Position;
layout (location = 1) in vec2 in_TexCoord;

out VertexData
{
	vec2 v_TexCoord;
};

void main()
{
	gl_Position = vec4(in_Position, 0.5, 1.0);
	v_TexCoord = in_TexCoord;
}


