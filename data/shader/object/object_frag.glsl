#version 430

in VertexData
{
	vec4 v_Color;
	vec2 v_TexCoord;
};

layout (location = 0) out vec4 out_Color;

layout (location = 0) uniform sampler2D u_Texture;

void main()
{
	out_Color = v_Color * texture(u_Texture, v_TexCoord);
}

