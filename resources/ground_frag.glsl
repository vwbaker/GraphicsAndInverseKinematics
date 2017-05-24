#version 330 core
uniform sampler2D Texture2;

in vec2 vTexCoord;
in vec3 vColor;

out vec4 Outcolor;

void main() {
  	vec4 texColor2 = texture(Texture2, vTexCoord);
	Outcolor = vec4(texColor2.r, texColor2.g, texColor2.b, 1); 
}

