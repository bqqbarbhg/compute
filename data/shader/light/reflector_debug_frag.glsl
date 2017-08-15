#version 430

layout (location = 0) out vec4 out_Color;

in VertexData
{
	vec3 v_Color;
};

void main()
{
	out_Color = vec4(v_Color, 1.0);
}

