//> all
#version 460 core

layout (location = 0) in vec3 inDirection;
layout (location = 0) out vec4 outColor;



layout(set = 1, binding = 1) uniform samplerCube skybox;



void main() {
    outColor = texture(skybox, inDirection);
}
