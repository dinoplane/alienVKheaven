//> all
#version 460 core

layout(set = 0, binding = 0) readonly uniform GPUSceneBuffer{ 
    mat4 worldFromCamera;
    mat4 cameraFromClip;
    mat4 clipFromWorld;
    vec4 lightDir;
} gpuSceneData;

layout (set = 1, binding = 0) readonly buffer VertexBuffer{ 
    vec4 skyboxVertices[];
} vertexBuffer;

layout (location = 0) out vec3 outDirection;

void main(){
    
    gl_Position = vertexBuffer.skyboxVertices[gl_VertexIndex];
    // vec3 direction = (gpuSceneData.worldFromClip * vec4(vec3(gl_FragCoord), 0.0)).xyz;
    vec4 worldPos = gpuSceneData.cameraFromClip * gl_Position;
    vec3 direction = mat3(gpuSceneData.worldFromCamera) * worldPos.xyz;

    outDirection = direction;

}
