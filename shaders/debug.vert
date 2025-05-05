//> all
#version 460 core

layout(set = 0, binding = 0) readonly uniform GPUSceneBuffer{ 
    mat4 worldFromCamera;
    mat4 cameraFromClip;
    mat4 clipFromWorld;
    vec4 lightDir;
    vec4 eyePos;
    uint enableShadows;
} gpuSceneData;

layout(set = 1, binding = 0) readonly buffer VertexBuffer{ 
	vec4 vertices[];
} vertexBuffer;


layout (set = 1, binding = 1) readonly buffer InstanceDataBuffer{ 
    mat4 lightTransformMatrices[];
} lightTransformsData;

void main() 
{	
    gl_Position = gpuSceneData.clipFromWorld * lightTransformsData.lightTransformMatrices[gl_InstanceIndex] * vertexBuffer.vertices[gl_VertexIndex];
}
//< all
