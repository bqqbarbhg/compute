#version 430

in VertexData
{
	vec2 v_TexCoord;
	vec3 v_Color;
};

layout (location = 0) out vec4 out_Color;
layout (location = 0) uniform sampler2D u_Texture;

void main()
{
	out_Color = vec4(v_Color, 1.0);
}


