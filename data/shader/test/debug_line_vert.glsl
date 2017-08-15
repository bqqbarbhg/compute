#version 430

layout (std140, binding=0) uniform Block
{
	mat4 u_ViewProjection;
};

layout (location=0) in vec4 in_Position;

void main()
{
	gl_Position = u_ViewProjection * in_Position;
}


