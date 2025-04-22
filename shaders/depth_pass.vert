//> all
#version 460 core
#extension GL_EXT_buffer_reference : require

#extension GL_EXT_multiview : enable

// layout (location = 0) out vec3 outColor;
// layout (location = 1) out vec2 outUV;
// layout (location = 2) out vec3 outPosition;
// layout (location = 3) out vec3 outNormal;
// layout (location = 4) out flat uint outTextureIndex;
// layout (location = 5) out flat uint outMaterialIndex;


struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

struct Primitive {
    uint materialIdx;
    uint textureIdx;
};

struct NodePrimitvePair {
    uint nodeIdx;
    uint primitiveIdx;
};

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};


//push constants block
layout( push_constant ) uniform constants
{
    uint shadowMapIdx;
    uint padding;
    uint padding2;
    uint padding3;
} PushConstants;

layout(set = 0, binding = 0) readonly uniform GPUSceneBuffer{ 
    mat4 worldFromCamera;
    mat4 cameraFromClip;
    mat4 clipFromWorld;
    vec4 lightDir;
    vec4 eyePos;
} gpuSceneData;


layout(set = 1, binding = 0) readonly buffer VertexBuffer{ 
	Vertex vertices[];
} vertexBuffer;

layout(set = 2, binding = 0) readonly buffer NodeTransformsBuffer{ 
    mat4 matrices[];
} nodeTransformsData;


layout(set = 2, binding = 1) readonly buffer PrimitiveBuffer{ 
    Primitive primitives[];
} primitiveData;

layout (set = 2, binding = 2) readonly buffer NodePrimitiveBuffer{ 
    NodePrimitvePair nodePrimitives[];
} nodePrimitiveData;

layout (set = 2, binding = 4) readonly buffer InstanceDataBuffer{ 
    mat4 modelMatrices[];
} instanceData;


layout (set = 3, binding = 0 ) readonly buffer lightSpaceMatricesBuffer {
    mat4 lightProjectionMatrix;
    mat4 lightSpaceMatrices[];
} lightSpaceMatricesData;


layout(set = 4, binding = 0) buffer readonly LightLengthBuffer{
    uint pointLightBufferLength;
};

layout(set = 4, binding = 1) buffer readonly PointLightBuffer{
    PointLight pointLights[];
};


void main() 
{	
    uint primitiveIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].primitiveIdx;
    Primitive primitive = primitiveData.primitives[primitiveIdx];
	Vertex v = vertexBuffer.vertices[gl_VertexIndex];

    uint nodeIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].nodeIdx;
    mat4 nodeTransform = nodeTransformsData.matrices[nodeIdx];

	//output data
    vec4 pos = instanceData.modelMatrices[gl_InstanceIndex] * nodeTransform * vec4(v.position, 1.0f);

    mat4 lightSpaceView = lightSpaceMatricesData.lightSpaceMatrices[gl_ViewIndex];
    lightSpaceView[3] = vec4(pointLights[PushConstants.shadowMapIdx].position, 1.0f);

    mat4 lightSpaceProjection = lightSpaceMatricesData.lightProjectionMatrix;

	gl_Position =  lightSpaceProjection * lightSpaceView * pos;
}
//< all
