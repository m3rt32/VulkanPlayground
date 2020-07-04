#version 450

layout(location=0) out vec4 outputColor;
layout(location=0) in vec4 color;
layout(location=1) in vec4 pos;
layout(location=2) in vec3 pass_normal;

void main(){
	vec3 lightPos = vec3(0,2,2);
	vec3 norm = normalize(pass_normal);
	vec3 lightDir = normalize(lightPos-pos.xyz);
	float diffuse = max(dot(norm,lightDir),0.0);
	outputColor = vec4(vec3(1.0)*diffuse*2.,1.0);
}