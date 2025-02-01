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
    std::vector<std::shared_ptr<Node>> children;
    std::shared_ptr<Node> parent;
    std::vector<uint32_t> meshIndices;
};

struct Mesh
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<MeshAsset> subMeshes;
    Sphere boundingVolume;
    std::vector<Node> nodes;
};

struct Primitive {
    uint32_t vertexStartIdx;
    uint32_t vertexCount;
    
    uint32_t indexStartIdx;
    uint32_t indexCount;
    
};

struct LoadedGLTF {

    // storage for all the data on a given glTF file
    std::vector<MeshAsset> meshes;
    std::vector<Primitive> primitives;
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



class Loader {
public:
    static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfModel(std::string_view filePath);
    static void LoadGltfMesh(fastgltf::Asset& gltf, Mesh* mesh);

    // static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfMaterial(VulkanEngine* engine,std::string_view filePath);
    // static std::optional<std::shared_ptr<LoadedGLTF>> LoadGltfImage(VulkanEngine* engine,std::string_view filePath);

};

