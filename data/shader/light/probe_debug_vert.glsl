#version 430

layout (std140, binding=0) uniform Global
{
	mat4 u_ViewProjection;
};

layout (std140, binding=1) uniform Block
{
	vec4 u_PositionRadius;
	vec4 u_SH[4];
};

layout (location=0) in vec3 in_Normal;

out VertexData
{
	vec3 v_Color;
};

void main()
{
	vec3 pos = u_PositionRadius.xyz + in_Normal * u_PositionRadius.w;
	gl_Position = u_ViewProjection * vec4(pos, 1.0);

	vec3 n = in_Normal;
	vec3 sh = u_SH[0].xyz;
	sh += u_SH[1].xyz * n.x;
	sh += u_SH[2].xyz * n.y;
	sh += u_SH[3].xyz * n.z;

	v_Color = max(sh, vec3(0.0));
}


