#version 430

in VertexData
{
	vec4 v_Color;
};

layout (location = 0) out vec4 out_Color;

void main()
{
	out_Color = v_Color;
}

