#version 450

//const vec3 vertices[3] = vec3[]
//(
//	vec3(0,0.5,0),
//	vec3(0.5,-0.5,0),
//	vec3(-0.5,-0.1,0)
//);

layout(location=0) in vec3 position;

layout(location=1) in vec3 normal;

layout(location=2) in vec3 texcoord;

layout(location=0) out vec4 color;
layout(location=1) out vec4 pos;
layout(location=2) out vec3 pass_normal;

void main(){
	gl_Position = vec4(position+vec3(0,0,-.25),1.0);
	color = vec4(normal*0.5+0.5,1.0); //normal unpack
	pos = vec4(position,1.0);
	pass_normal = normal;
}