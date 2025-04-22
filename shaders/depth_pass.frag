//glsl version 4.5
#version 460 core


layout (location = 0) in vec3 inPosition;
// layout (depth_less) out float gl_FragDepth;

layout( push_constant ) uniform constants
{
    uint shadowMapIdx;
    uint padding;
    uint padding2;
    uint padding3;
} PushConstants;

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(set = 4, binding = 1) buffer readonly PointLightBuffer{
    PointLight pointLights[];
};


void main() 
{
    vec3 lightPos = pointLights[PushConstants.shadowMapIdx].position;
    vec3 worldSpaceFragPos = inPosition.xyz;

    float worldSpaceDistance = distance(worldSpaceFragPos, lightPos);
    float normalizedDistance = worldSpaceDistance / 10.0f;

    // gl_FragDepth = normalizedDistance;
}
