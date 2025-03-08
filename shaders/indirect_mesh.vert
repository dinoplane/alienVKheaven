//> all
#version 460 core
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outPosition;
layout (location = 3) out vec3 outNormal;
layout (location = 4) out flat uint outTextureIndex;
layout (location = 5) out flat uint outMaterialIndex;


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


//push constants block
// layout( push_constant ) uniform constants
// {	
// 	mat4 clipFromWorld;
//     vec4 lightDir;
// } PushConstants;


void main() 
{	
    uint primitiveIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].primitiveIdx;
    Primitive primitive = primitiveData.primitives[primitiveIdx];
    // uint indiceIdx = primitive.indexStartIdx + gl_VertexIndex;
    // uint vertexIdx = PushConstants.indexBuffer.indices[indiceIdx];
    //load vertex data from device address
	Vertex v = vertexBuffer.vertices[gl_VertexIndex];


    uint nodeIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].nodeIdx;
    mat4 nodeTransform = nodeTransformsData.matrices[nodeIdx];
    // mat4 nodeTransform = mat4(1.0f);

	//output data
    vec4 pos = instanceData.modelMatrices[gl_InstanceIndex] * nodeTransform * vec4(v.position, 1.0f);
	gl_Position = gpuSceneData.clipFromWorld * pos;
	outColor = v.normal.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outPosition = pos.xyz;
    vec4 norm = instanceData.modelMatrices[gl_InstanceIndex] * nodeTransform * vec4(v.normal, 0.0f);
	outNormal = normalize(norm.xyz);
    // outNormal = v.normal;

    outTextureIndex = primitive.textureIdx;
    outMaterialIndex = primitive.materialIdx;
}
//< all
