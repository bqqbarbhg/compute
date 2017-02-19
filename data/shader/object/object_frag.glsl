#version 430

in VertexData
{
	vec4 v_Color;
	vec2 v_TexCoord;
};

layout (location = 0) out vec4 out_Color1;
layout (location = 1) out vec4 out_Color2;

layout (location = 0) uniform sampler2D u_Texture;

void main()
{
	out_Color1 = v_Color;
	out_Color2 = texture(u_Texture, v_TexCoord);
}

