#version 430

in VertexData
{
	vec3 v_Color;
};

layout (location = 0) out vec4 out_Color;

void main()
{
	out_Color = vec4(v_Color, 1.0);
}

