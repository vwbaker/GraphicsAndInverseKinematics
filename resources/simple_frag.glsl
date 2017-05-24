#version 330 core 
in vec3 fragNor;
in vec3 WPos;
in vec3 fragTex;
in vec3 Dlight;
in vec2 vTexCoord;

out vec4 color;

uniform sampler2D catTexture;
uniform vec3 MatAmb;
uniform vec3 MatDif;
uniform vec3 MatSpec;
uniform float shine;

uniform vec3 center;
uniform vec3 viewVector;

void main()
{
	vec3 lightDir = normalize(Dlight);
	vec3 Dcolor; 
	vec3 Lcolor = vec3(2.0, 2.0, 2.0);
	vec3 normal = normalize(fragNor);
	vec3 view = normalize(-WPos);
	vec3 halfV = normalize(view + lightDir);

	Dcolor = MatDif * max(dot(lightDir, normal), 0) * Lcolor;
	Dcolor += MatSpec * pow(max(dot(halfV, normal), 0), shine) * Lcolor;
	Dcolor += MatAmb * Lcolor;
	color = vec4(Dcolor, 1.0);

	vec4 texColor = texture(catTexture, vTexCoord);
	color = vec4(texColor.g * Dcolor.r, texColor.b * Dcolor.g, texColor.r * Dcolor.b, 1);
}
