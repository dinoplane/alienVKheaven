// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

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
    glm::mat4 matrix;
    uint32_t meshIndex;

    std::vector<std::shared_ptr<Node>> children;
    std::shared_ptr<Node> parent;
    std::vector<uint32_t> meshIndices;
};

struct LoadedPrimitive {
    uint32_t vertexStartIdx;
    uint32_t vertexCount;
    
    uint32_t indexStartIdx;
    uint32_t indexCount;
};


struct LoadedMesh
{    
    uint32_t primitiveStartIdx;
    uint32_t primitiveCount;

    uint32_t nodeStartIdx;
    uint32_t nodeCount;
};


struct LoadedGLTF {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;


    // storage for all the data on a given glTF file
    std::vector<LoadedMesh> meshes;
    std::vector<LoadedPrimitive> primitives;
    std::vector<Node> nodes;
    // std::vector<AllocatedImage> images;
    // std::vector<GLTFMaterial> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<Node> topNodes;

    std::vector<VkSampler> samplers;

    // DescriptorAllocator descriptorPool;

    // AllocatedBuffer materialDataBuffer;

    // VulkanEngine* creator;

    ~LoadedGLTF() { clearAll(); };

    // virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:

    void clearAll();
};


/*
TODO: Because static geometry doesn't change at all, you could just create 2 huge buffers with all data in world space and upload them to the GPU.
TODO: Because dynamic geometry changes a lot, make sure that matrices that change also change the children as well (CPU work)?
*/

class Loader {
public:
    static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfModel(const std::string_view filePath);
    static bool LoadGltfMesh(const fastgltf::Asset& gltfAsset, const fastgltf::Mesh& gltfMesh,
                                LoadedGLTF* outModel, LoadedMesh* outMesh, 
                                std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);


    static bool LoadGltfNode(const fastgltf::Asset& gltf, const fastgltf::Node& gltfNode, 
                                LoadedGLTF* outModel, Node* outNode);
    // static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfMaterial(VulkanEngine* engine,std::string_view filePath);
    // static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfImage(VulkanEngine* engine,std::string_view filePath);

};

