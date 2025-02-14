//glsl version 4.5
#version 460 core

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 inNormal;

//output write
layout (location = 0) out vec4 outFragColor;


layout(rgba16f,set = 2, binding = 0) sampler2D positionImage;
layout(rgba16f,set = 2, binding = 1) sampler2D normalImage;
layout(rgba16f,set = 2, binding = 2) sampler2D albedoSpecImage;


vec3 lightPos = vec3(10.0, 10.0, 10.0); 

void main() 
{

	vec3 norm = normalize(inNormal);
	vec3 lightDir = normalize(lightPos - inPosition);
	float diff = max(dot(norm, lightDir), 0.0);

	outFragColor = diff * vec4(1.0);
	outFragColor = vec4(inNormal, 1.0);

    ivec2 pixel = ivec2(gl_FragCoord.xy);
    positionImage.write(nodeTransformsData.matrices[0] * vec4(inPosition, 1.0), pixel);

}
