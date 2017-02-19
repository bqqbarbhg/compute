#version 430

layout (std140, binding=0) uniform Block
{
	vec2 u_Offset;
};

layout (location=0) in vec4 in_Position;

out VertexData
{
	vec2 v_TexCoord;
};

void main()
{
	gl_Position = in_Position + vec4(u_Offset.xy, 0.0, 0.0);
	v_TexCoord = in_Position.xy;
}

