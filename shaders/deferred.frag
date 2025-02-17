//glsl version 4.5
#version 460 core


#extension GL_EXT_nonuniform_qualifier : enable

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in flat uint inTextureIdx;
layout (location = 5) in flat uint inMaterialIdx;


//output write
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedoSpec;

struct Material {
    vec4 baseColorFactor;
    float alphaCutoff;
    uint flags;
};

layout (set = 1, binding = 3) readonly buffer MaterialBuffer{ 
    Material materials[];
} materialData;


layout(set = 2, binding = 0) uniform sampler2D textures[];


vec3 lightPos = vec3(10.0, 10.0, 10.0); 

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}


void main() 
{
	outPosition = vec4(inPosition, 1.0);
	outNormal = vec4(inNormal, 1.0);
	outAlbedoSpec = texture(textures[inTextureIdx], inUV);

    Material material = materialData.materials[inMaterialIdx];

    // float factor = (rand(gl_FragCoord.xy) - 0.5) / 8;
    if (outAlbedoSpec.a < material.alphaCutoff)
        discard;
    else {
        // outAlbedoSpec = vec4(1.0);
    }
}
