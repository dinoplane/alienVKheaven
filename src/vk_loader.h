// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

class VulkanEngine;

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
   
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

struct Node 
{
    glm::mat4 worldTransform; // world transform of the node to local/model space

    glm::mat4 localTransform; // local transform of the node to parent space
    
    uint32_t meshIdx;
    uint32_t parentIdx;
    std::vector<uint32_t> children;

};

struct LoadedPrimitive {
    int32_t vertexStartIdx;
    uint32_t vertexCount;
    
    uint32_t indexStartIdx;
    uint32_t indexCount;
};

struct PrimitiveProperties {
    uint32_t materialIdx;
    uint32_t textureIdx;
};

struct LoadedMesh
{    
    uint32_t primitiveStartIdx;
    uint32_t primitiveCount;

    uint32_t nodeStartIdx;
    uint32_t nodeCount;
};

struct NodePrimitivePair {
    uint32_t nodeIdx;
    uint32_t primIdx;
};

struct ModelData {
    uint32_t imageStartIdx;
    uint32_t imageCount;

    uint32_t materialStartIdx;
    uint32_t materialCount;

    uint32_t meshStartIdx;
    uint32_t meshCount;

    uint32_t nodeStartIdx;
    uint32_t nodeCount;

    uint32_t primitiveStartIdx;
    uint32_t primitiveCount;

    uint32_t nodePrimitivePairStartIdx;
    uint32_t nodePrimitivePairCount;

    uint32_t topNodeStartIdx;
    uint32_t topNodeCount;

    uint32_t drawCmdBufferStartIdx;
    uint32_t drawCmdBufferCount;

    uint32_t instanceTransformsStartIdx;
    uint32_t instanceTransformsCount;
};

enum MaterialUniformFlags : std::uint32_t {
    None = 0 << 0,
    HasBaseColorTexture = 1 << 0,
};

struct LoadedMaterial {
    glm::vec4 baseColorFactor;
    float alphaCutoff;
    uint32_t flags;
};

struct LoadedGLTF { // holds all the data for a group of gltf files
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;


    // storage for all the data on a given glTF file
    std::vector<LoadedMesh> meshes;
    std::vector<LoadedPrimitive> primitives;
    std::vector<PrimitiveProperties> primitivePropertiesBufferVec;
    std::vector<Node> nodes;
    std::vector<NodePrimitivePair> nodePrimitivePairs;
    std::vector<glm::mat4> nodeTransforms;
    std::vector<AllocatedImage> images;
    std::vector<LoadedMaterial> materials;
    std::vector<glm::mat4> instanceTransforms;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<uint32_t> topNodes;
    
    std::vector<VkDrawIndexedIndirectCommand> drawCmdBufferVec;
    uint32_t drawCount;

    std::vector<ModelData> modelDataVec;


    void refreshTransform(const glm::mat4& parentMatrix, uint32_t nodeIdx) 
    {
        Node& node = nodes[nodeIdx];
        node.worldTransform = parentMatrix * node.localTransform;
        for (auto c : node.children) {
            refreshTransform(node.worldTransform, c);
        }
    }

    void ClearAll();
};


/*
TODO: Because static geometry doesn't change at all, you could just create 2 huge buffers with all data in world space and upload them to the GPU.
TODO: Because dynamic geometry changes a lot, make sure that matrices that change also change the children as well (CPU work)?
*/


struct BufferCreateParameter{
    size_t allocSize;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memoryUsage;
    void* data;
};

class Loader {
public:
    // static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfModel(const std::string_view filePath);

    static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfModel(VulkanEngine* engine, const std::unordered_map<std::string, std::vector<glm::mat4>>& filePathsToInstances);


    static bool LoadGltfMesh(const fastgltf::Asset& gltfAsset, const fastgltf::Mesh& gltfMesh,
                                LoadedGLTF* outModel, LoadedMesh* outMesh, 
                                std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
                                std::vector<LoadedPrimitive>* primitives, std::vector<PrimitiveProperties>* primitiveProperties,
                                size_t baseImageIdx, size_t baseMaterialIdx);

    static bool LoadGltfImage(VulkanEngine* engine, const std::string filePath,  const fastgltf::Asset& gltfAsset, const fastgltf::Image& image, LoadedGLTF* outModel, AllocatedImage* outImage);
    static bool LoadGltfMaterial(const fastgltf::Asset& gltfAsset, const fastgltf::Material& material, LoadedMaterial* outMaterial);

    // static bool LoadGltfNode(const fastgltf::Asset& gltf, const fastgltf::Node& gltfNode, 
    //                             LoadedGLTF* outModel, Node* outNode);

    static GPUModelBuffers LoadGeometryFromGLTF(const LoadedGLTF& inModel, VulkanEngine* engine);

    static AllocatedImage LoadCubeMap(const std::vector<std::string> paths, VulkanEngine* engine);
    static void PrintModelData(const LoadedGLTF& modelData);
    static void DestroyModelData(const GPUModelBuffers& inModelBuffers, VulkanEngine* engine);

    VulkanEngine* engine;


    std::vector<BufferCreateParameter> creationParameters;
    size_t totalStagingBufferSize;


    void AddBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data);

    void Init(VulkanEngine* inEngine);
    std::vector<AllocatedBuffer> UploadBuffers();
    void Clear();
};

