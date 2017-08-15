#version 430

in VertexData
{
	vec2 v_TexCoord;
	float v_Fade;
};

layout (location = 0) out vec4 out_Color;

layout (binding = 0) uniform sampler2D u_Texture;
layout (binding = 1) uniform sampler2D u_TextureFade;

void main()
{
	vec4 lineSample = texture(u_Texture, v_TexCoord);

	if (v_Fade > 0.01) {
		vec4 fadeSample = texture(u_TextureFade, v_TexCoord);
		out_Color = mix(lineSample, fadeSample, v_Fade);
	} else {
		out_Color = lineSample;
	}
}

