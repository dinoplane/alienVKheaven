//> all
#version 460 core
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outPosition;
layout (location = 3) out vec3 outNormal;

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

struct Primitive {
    int vertexStartIdx;
    uint vertexCount;
    
    uint indexStartIdx;
    uint indexCount;
};

struct NodePrimitvePair {
    uint nodeIdx;
    uint primitiveIdx;
};


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer IndexBuffer{ 
    uint indices[];
};

layout(set = 0, binding = 0) readonly buffer NodeTransformsBuffer{ 
    mat4 matrices[];
} nodeTransformsData;


layout(set = 0, binding = 1) readonly buffer primitiveBuffer{ 
    Primitive primitives[];
} primitiveData;

layout (set = 0, binding = 2) readonly buffer nodePrimitiveBuffer{ 
    NodePrimitvePair nodePrimitives[];
} nodePrimitiveData;

//push constants block
layout( push_constant ) uniform constants
{	
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
} PushConstants;



void main() 
{	
    uint primitiveIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].primitiveIdx;
    Primitive primitive = primitiveData.primitives[primitiveIdx];
    uint indiceIdx = primitive.indexStartIdx + gl_VertexIndex;
    uint vertexIdx = PushConstants.indexBuffer.indices[indiceIdx];
    //load vertex data from device address
	Vertex v = PushConstants.vertexBuffer.vertices[vertexIdx];


    uint nodeIdx = nodePrimitiveData.nodePrimitives[gl_DrawID].nodeIdx;
    mat4 nodeTransform = nodeTransformsData.matrices[nodeIdx];
    // mat4 nodeTransform = mat4(1.0f);

	//output data
	gl_Position = PushConstants.render_matrix * nodeTransform * vec4(v.position, 1.0f);
	outColor = v.normal.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outPosition = v.position;
	outNormal = v.normal;
}
//< all
