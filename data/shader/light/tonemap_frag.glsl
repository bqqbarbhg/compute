#version 430

in VertexData
{
	vec2 v_TexCoord;
};

layout (location = 0) out vec4 out_Color;
layout (binding = 0) uniform sampler2D u_HdrTexture;

layout (std140, binding=0) uniform Block
{
	vec4 u_Tonemap; // Exposure, White
	vec4 u_Filmic;  // x0, y0, x1, y1
};


void main()
{
	vec3 hdr = texture2D(u_HdrTexture, v_TexCoord).rgb;

	hdr *= 1.5;

	hdr = pow(hdr, vec3(1/2.2));

	out_Color = vec4(hdr, 1.0);
}


