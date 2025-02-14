//glsl version 4.5
#version 460 core

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 inNormal;

//output write
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedoSpec;


vec3 lightPos = vec3(10.0, 10.0, 10.0); 

void main() 
{
	outPosition = vec4(inPosition, 1.0);
	outNormal = vec4(inNormal, 1.0);
	outAlbedoSpec = vec4(1.0);
}
