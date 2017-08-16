#version 430

layout (std140, binding=0) uniform Block
{
	mat4 u_ViewProjection;
};

layout (location=0) in vec3  in_Position;
layout (location=1) in vec3  in_Normal;
layout (location=2) in float in_Radius;
layout (location=3) in vec3  in_Light;
layout (location=4) in vec4  in_Color;
layout (location=5) in vec2  in_Vert;

out VertexData
{
	vec3 v_Color;
};

void main()
{
	vec3 up = abs(in_Normal.y) > 0.5 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
	vec3 tangent = normalize(cross(in_Normal, up));
	vec3 bitangent = normalize(cross(in_Normal, tangent));

	vec3 pos = in_Position + (tangent * in_Vert.x + bitangent * in_Vert.y) * in_Radius;

	v_Color = in_Color.rgb;
	// v_Color = in_Light.rgb;

	gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

