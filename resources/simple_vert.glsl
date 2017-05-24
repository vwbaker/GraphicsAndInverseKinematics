#version  330 core
layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;
//layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 M;
uniform mat4 V;
uniform vec3 center;
uniform vec3 viewVector;

out vec3 fragNor;
out vec3 WPos;
out vec3 fragTex;
out vec3 Dlight;
out vec2 vTexCoord;

void main()
{
	vec2 vertTex;
	gl_Position = P * V * M * vertPos;
	fragNor = (V * M * vec4(vertNor, 0.0)).xyz;
	WPos = vec3(M * vertPos);
	Dlight = (V * vec4(3, 6, 1, 0)).xyz;

	vec2 center2d = vec2(center.x, center.z);
	vec2 wpos2d = vec2(WPos.x, WPos.z);
	float theta = acos(dot(normalize(vec2(viewVector.x, viewVector.z)), 
		normalize(wpos2d - center2d)));
	vTexCoord = vec2(cos(theta) * distance(wpos2d, center2d),
		sin(theta) * distance(wpos2d, center2d));
}
