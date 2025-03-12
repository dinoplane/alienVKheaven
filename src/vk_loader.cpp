#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <vk_loader.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "vk_mem_alloc.h"


std::optional<std::shared_ptr<LoadedGLTF>> Loader::LoadGltfModel(VulkanEngine* engine, const std::unordered_map<std::string, std::vector<glm::mat4>>& filePathsToInstances){
    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    LoadedGLTF& loadedModel = *scene.get();
    fastgltf::Parser parser = fastgltf::Parser(
        fastgltf::Extensions::KHR_materials_specular 
        | fastgltf::Extensions::KHR_materials_transmission 
        | fastgltf::Extensions::KHR_materials_ior 
        | fastgltf::Extensions::KHR_materials_volume
        | fastgltf::Extensions::KHR_materials_dispersion);

    // Create a default material
    loadedModel.materials.push_back(LoadedMaterial{});
    auto& defaultMaterial = loadedModel.materials[0];
    defaultMaterial.baseColorFactor = glm::vec4(1.0f);
    defaultMaterial.alphaCutoff = 0.0f;
    defaultMaterial.flags = 0;

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	AllocatedImage _errorCheckerboardImage = engine->CreateImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

    loadedModel.images.push_back(_errorCheckerboardImage);

    for (auto& filePathPair : filePathsToInstances){
        std::string filePath = filePathPair.first;
        fmt::print("Loading GLTF: {}\n", filePath);

        constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
        // fastgltf::Options::LoadExternalImages;


        fastgltf::Asset gltfAsset;

        std::filesystem::path path = filePath;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
        if (!bool(gltfFile)) {
            std::cout << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
            assert(false);
        }

        auto asset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
        if (asset.error() != fastgltf::Error::None) {
            std::cout << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
            assert(false);
        }
        gltfAsset = std::move(asset.get());

        size_t initMeshesSize = loadedModel.meshes.size();
        size_t initPrimitiveSize = loadedModel.primitives.size();
        size_t initVerticesSize = loadedModel.vertices.size();
        size_t initIndicesSize = loadedModel.indices.size();
        size_t initNodesSize = loadedModel.nodes.size();
        size_t initImagesSize = loadedModel.images.size();
        size_t initMaterialsSize = loadedModel.materials.size();
        size_t initTopNodesSize = loadedModel.topNodes.size();

        // Load Images
        loadedModel.images.resize(initImagesSize + gltfAsset.images.size());
        for ( size_t imageIdx = 0; imageIdx < gltfAsset.images.size(); ++imageIdx ){
            const fastgltf::Image& gltfImage = gltfAsset.images[imageIdx];
            AllocatedImage& loadedImage = loadedModel.images[initImagesSize + imageIdx];
            if (!Loader::LoadGltfImage(engine, filePath, gltfAsset, gltfImage, &loadedModel, &loadedImage)){
                assert(false);
            }
        }

        // Load Materials
    	loadedModel.materials.resize(initMaterialsSize + gltfAsset.materials.size());
        for ( size_t materialIdx = 0; materialIdx < gltfAsset.materials.size(); ++materialIdx ){
            const fastgltf::Material& gltfMaterial = gltfAsset.materials[materialIdx];
            LoadedMaterial& loadedMaterial = loadedModel.materials[initMaterialsSize + materialIdx];
            if (!Loader::LoadGltfMaterial(gltfAsset, gltfMaterial, &loadedMaterial)){
                assert(false);
            }
            fmt::print("Material: {}\n", initMaterialsSize + materialIdx);
            fmt::print("Alpha Cutoff: {}\n", loadedMaterial.alphaCutoff);
        }

        //Load Meshes
        loadedModel.meshes.resize(initMeshesSize + gltfAsset.meshes.size());
        for ( size_t meshIdx = 0; meshIdx < gltfAsset.meshes.size(); ++meshIdx ){
            const fastgltf::Mesh& gltfMesh = gltfAsset.meshes[meshIdx];
            LoadedMesh& loadedMesh = loadedModel.meshes[meshIdx];
            
            if (!Loader::LoadGltfMesh(gltfAsset, gltfMesh,
                 &loadedModel, &loadedMesh, 
                 &loadedModel.vertices, &loadedModel.indices, 
                 &loadedModel.primitives, &loadedModel.primitivePropertiesBufferVec, 
                 initImagesSize, initMaterialsSize)){
                assert(false);
            }
        }

        // Create scene graph by iterating through the nodes.
        fastgltf::iterateSceneNodes(gltfAsset, 0u, fastgltf::math::fmat4x4(),
                                    [&](const fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {
                                        
            glm::mat4 worldTransform = glm::make_mat4(matrix.data());
            glm::mat4 localTransform = glm::make_mat4(fastgltf::getTransformMatrix(node).data());
            if (node.meshIndex.has_value()) {
                loadedModel.nodes.push_back( Node{worldTransform, localTransform, static_cast<uint32_t>(*node.meshIndex), UINT32_MAX} );
            } else {
                loadedModel.nodes.push_back( Node{worldTransform, localTransform, UINT32_MAX, UINT32_MAX} );
            }
            loadedModel.nodeTransforms.push_back(worldTransform);
        });

        for (size_t nodeIdx = 0; nodeIdx < gltfAsset.nodes.size(); ++nodeIdx) {
            fastgltf::Node& node = gltfAsset.nodes[nodeIdx];

            for(auto& childIdx : node.children){
                loadedModel.nodes[initNodesSize + nodeIdx].children.push_back(childIdx);
                loadedModel.nodes[initNodesSize + childIdx].parentIdx = nodeIdx;
            }
        }

        for (size_t nodeIdx = initNodesSize; nodeIdx < loadedModel.nodes.size(); ++nodeIdx) {
            Node& node = loadedModel.nodes[nodeIdx];
            if (node.parentIdx == UINT32_MAX) {
                loadedModel.topNodes.push_back(nodeIdx);
                loadedModel.refreshTransform(glm::mat4(1.0f), nodeIdx);
            }
        }

        size_t initInstanceTransformsSize = loadedModel.instanceTransforms.size();
        loadedModel.instanceTransforms.reserve(initInstanceTransformsSize + filePathPair.second.size());
        loadedModel.instanceTransforms.insert(loadedModel.instanceTransforms.end(), filePathPair.second.begin(), filePathPair.second.end());

        // Generate draw calls by iterating through nodes across primitives.
        size_t initNodePrimitivePairSize = loadedModel.nodePrimitivePairs.size();
        size_t initDrawCmdBufferVecSize = loadedModel.drawCmdBufferVec.size();
        for ( uint32_t nodeIdx = initNodesSize; nodeIdx < loadedModel.nodes.size(); ++nodeIdx )
        {
            uint32_t meshIdx = loadedModel.nodes[nodeIdx].meshIdx;
            if (meshIdx == UINT32_MAX) continue;

            const LoadedMesh& mesh = loadedModel.meshes[meshIdx];

            for (uint32_t primOffset = 0; primOffset < mesh.primitiveCount; ++primOffset)
            {
                const uint32_t primIdx = mesh.primitiveStartIdx + primOffset;
                loadedModel.nodePrimitivePairs.push_back( { nodeIdx,  primIdx } );
                const LoadedPrimitive& prim = loadedModel.primitives[ primIdx ];
                loadedModel.drawCmdBufferVec.push_back ( VkDrawIndexedIndirectCommand{
                    prim.indexCount, 		// indexCount
                    static_cast<uint32_t>(filePathPair.second.size()),						// instanceCount
                    prim.indexStartIdx,		// firstIndex
                    prim.vertexStartIdx,	// vertexOffset 
                    static_cast<uint32_t>(initInstanceTransformsSize)						// firstInstance
                } );
            }
        } 

        loadedModel.modelDataVec.push_back( ModelData{
            static_cast<uint32_t>(initImagesSize), static_cast<uint32_t>(loadedModel.images.size() - initImagesSize),
            static_cast<uint32_t>(initMaterialsSize), static_cast<uint32_t>(loadedModel.materials.size() - initMaterialsSize),
            static_cast<uint32_t>(initMeshesSize), static_cast<uint32_t>(gltfAsset.meshes.size()),
            static_cast<uint32_t>(initNodesSize), static_cast<uint32_t>(gltfAsset.nodes.size()),
            static_cast<uint32_t>(initPrimitiveSize), static_cast<uint32_t>(loadedModel.primitives.size() - initPrimitiveSize),
            static_cast<uint32_t>(initNodePrimitivePairSize), static_cast<uint32_t>(loadedModel.nodePrimitivePairs.size() - initNodePrimitivePairSize),
            static_cast<uint32_t>(initTopNodesSize), static_cast<uint32_t>(loadedModel.topNodes.size() - initTopNodesSize),
            static_cast<uint32_t>(initDrawCmdBufferVecSize), static_cast<uint32_t>(loadedModel.drawCmdBufferVec.size() - initDrawCmdBufferVecSize),
            static_cast<uint32_t>(initInstanceTransformsSize), static_cast<uint32_t>(filePathPair.second.size())
        } );
    }

    return scene;    
}


bool Loader::LoadGltfMesh(const fastgltf::Asset& gltfAsset, const fastgltf::Mesh& gltfMesh,
                        LoadedGLTF* outModel, LoadedMesh* outMesh, 
                        std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
                        std::vector<LoadedPrimitive>* primitives, std::vector<PrimitiveProperties>* primitiveProperties,
                        size_t baseImageIdx, size_t baseMaterialIdx){
    
    size_t initPrimitivesSize = primitives->size();
	outMesh->primitiveStartIdx = initPrimitivesSize;
	outMesh->primitiveCount = static_cast<uint32_t>(gltfMesh.primitives.size());
    primitives->resize(initPrimitivesSize + gltfMesh.primitives.size());
    primitiveProperties->resize(initPrimitivesSize + gltfMesh.primitives.size());

    for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
        const auto* it = &gltfMesh.primitives[primIdx];
		LoadedPrimitive& outPrimitive = (*primitives)[initPrimitivesSize + primIdx];
        PrimitiveProperties& outPrimitiveProperties = (*primitiveProperties)[initPrimitivesSize + primIdx];

        if (it->materialIndex.has_value()) {
			outPrimitiveProperties.materialIdx = baseMaterialIdx + it->materialIndex.value();

            auto& material = gltfAsset.materials[it->materialIndex.value()];

			auto& baseColorTexture = material.pbrData.baseColorTexture;
            if (baseColorTexture.has_value()) {
                auto& texture = gltfAsset.textures[baseColorTexture->textureIndex];
				if (!texture.imageIndex.has_value())
					return false;

				outPrimitiveProperties.textureIdx = baseImageIdx + texture.imageIndex.value();
            }
        } else {
			outPrimitiveProperties.materialIdx = 0;
		}

        auto* positionIt = it->findAttribute("POSITION");
        assert(positionIt != it->attributes.end()); // A mesh primitive is required to hold the POSITION attribute.
        assert(it->indicesAccessor.has_value()); // We specify GenerateMeshIndices, so we should always have indices

        auto& positionAccessor = gltfAsset.accessors[positionIt->accessorIndex];
        if (!positionAccessor.bufferViewIndex.has_value())
            continue;

        size_t initVerticesSize = vertices->size();
        outPrimitive.vertexStartIdx = initVerticesSize;
        outPrimitive.vertexCount = positionAccessor.count;
        vertices->resize(initVerticesSize + positionAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, positionAccessor, [&](glm::vec3 pos, std::size_t idx) {
            ( *vertices )[ initVerticesSize + idx ].position = pos;
            ( *vertices )[ initVerticesSize + idx ].color = glm::vec4(1.0f);
        });
    
        {
            auto texcoordAttribute = std::string("TEXCOORD_0");// + std::to_string(baseColorTexcoordIndex);
            if (const auto* texcoord = it->findAttribute(texcoordAttribute); texcoord != it->attributes.end()) {
                auto& texCoordAccessor = gltfAsset.accessors[texcoord->accessorIndex];
                if (!texCoordAccessor.bufferViewIndex.has_value())
                    continue;

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltfAsset, texCoordAccessor, [&](glm::vec2 uv, std::size_t idx) {
                    ( *vertices )[ initVerticesSize + idx ].uv_x = uv.x;
                    ( *vertices )[ initVerticesSize + idx ].uv_y = uv.y;
                });
            }
        }

        {		
            auto normalAttribute = std::string("NORMAL");
            if (const auto* normal = it->findAttribute(normalAttribute); normal != it->attributes.end()) {
                auto& normalAccessor = gltfAsset.accessors[normal->accessorIndex];
                if (!normalAccessor.bufferViewIndex.has_value())
                    continue;

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, normalAccessor, [&](glm::vec3 norm, std::size_t idx) {
                    ( *vertices )[ initVerticesSize + idx ].normal.x = norm.x;
                    ( *vertices )[ initVerticesSize + idx ].normal.y = norm.y;
                    ( *vertices )[ initVerticesSize + idx ].normal.z = norm.z;
                });
            }
        }
        auto& indexAccessor = gltfAsset.accessors[it->indicesAccessor.value()];
		size_t initIndicesSize = indices->size();
		outPrimitive.indexStartIdx = initIndicesSize;
		outPrimitive.indexCount = indexAccessor.count;
		indices->resize( initIndicesSize + indexAccessor.count );

		fastgltf::iterateAccessorWithIndex<uint32_t>(gltfAsset, indexAccessor, [&](uint32_t indiceIdx, std::size_t idx) {
				( *indices )[ initIndicesSize + idx ] = indiceIdx;
		});
    }

    return true;
}

//TODO:  kinda dont like doing this but it is what it is. Find a solution to make this loader more independent later.
bool Loader::LoadGltfImage(VulkanEngine* engine,const std::string rootFilePath,  const fastgltf::Asset& gltfAsset,  const fastgltf::Image& image, LoadedGLTF* outModel, AllocatedImage* outImage) {
    auto getLevelCount = [](int width, int height) -> uint32_t {
        return static_cast<uint32_t>(1 + floor(log2(width > height ? width : height)));
    };

    std::visit(fastgltf::visitor {
        [](auto& arg) {
			fmt::print("Loaded Image: {}\n", "Nothing");
		},
        [&](const fastgltf::sources::URI& filePath) {
            assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
            assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.
            int width, height, nrChannels;

            const std::string uriPath(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.

            

            const std::string path = rootFilePath.substr(0, rootFilePath.find_last_of("/\\") + 1) + uriPath;
            unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4); 

            VkExtent3D extent = {width, height, 1};
            *outImage = engine->CreateImage(
                    data,
                    extent,
                    VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false
                );

            stbi_image_free(data);
			fmt::print("Loaded Image: {}\n", path);

        },
        [&](const fastgltf::sources::Array& vector) {
            int width, height, nrChannels;
            unsigned char *data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
	
            VkExtent3D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
            *outImage = engine->CreateImage(
                    data,
                    extent,
                    VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false
            );
            stbi_image_free(data);
			fmt::print("Loaded Image: {}\n", "From Memory");

        },
        [&](const fastgltf::sources::BufferView& view) {
            auto& bufferView = gltfAsset.bufferViews[view.bufferViewIndex];
            auto& buffer = gltfAsset.buffers[bufferView.bufferIndex];
            std::visit(fastgltf::visitor {
                [](auto& arg) {},
                [&](const fastgltf::sources::Array& vector) {
                    int width, height, nrChannels;
					unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
					static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);

                    VkExtent3D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
                    *outImage = engine->CreateImage(
                            data,
                            extent,
                            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false
                        );
                    stbi_image_free(data);
                }
            }, buffer.data);
			fmt::print("Loaded Image: {}\n", "From BufferView");

        },
    }, image.data);

    return true;
}

bool Loader::LoadGltfMaterial(const fastgltf::Asset& gltfAsset, const fastgltf::Material& material, LoadedMaterial* outMaterial) {
    outMaterial->alphaCutoff = material.alphaCutoff;

    outMaterial->baseColorFactor.x = material.pbrData.baseColorFactor.x();
	outMaterial->baseColorFactor.y = material.pbrData.baseColorFactor.y();
	outMaterial->baseColorFactor.z = material.pbrData.baseColorFactor.z();
	outMaterial->baseColorFactor.w = material.pbrData.baseColorFactor.w();
	outMaterial->flags = 0;
    if (material.pbrData.baseColorTexture.has_value()) {
        outMaterial->flags |= MaterialUniformFlags::HasBaseColorTexture;
    }
	fmt::print("Has Base Color Texture: {}\n", outMaterial->flags & MaterialUniformFlags::HasBaseColorTexture);
    return true;
}

void LoadedGLTF::ClearAll(){
    meshes.clear();
    primitives.clear();
    vertices.clear();
    indices.clear();
    nodes.clear();
    nodeTransforms.clear();
    nodePrimitivePairs.clear();
    topNodes.clear();
    drawCmdBufferVec.clear();
    modelDataVec.clear();
    images.clear();
    materials.clear();
    primitivePropertiesBufferVec.clear();
    instanceTransforms.clear();
}

GPUModelBuffers Loader::LoadGeometryFromGLTF(const LoadedGLTF& inModel, VulkanEngine* engine){
    // I feel like uploading buffers is a common operation... we could abstract this code into a helper function/class
    Loader loader;

    loader.Init(engine);

    const size_t vertexBufferSize = inModel.vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = inModel.indices.size() * sizeof(uint32_t);
    const size_t nodeTransformBufferSize = inModel.nodeTransforms.size() * sizeof(glm::mat4);
    const size_t primitiveBufferSize = inModel.primitivePropertiesBufferVec.size() * sizeof(PrimitiveProperties);
    const size_t nodePrimitivePairBufferSize = inModel.nodePrimitivePairs.size() * sizeof(NodePrimitivePair);
    const size_t materialBufferSize = inModel.materials.size() * sizeof(LoadedMaterial);
    const size_t drawCmdBufferVecBufferSize = inModel.drawCmdBufferVec.size() * sizeof(VkDrawIndexedIndirectCommand);
    const size_t instanceTransformBufferSize = inModel.instanceTransforms.size() * sizeof(glm::mat4);

    // create buffer on GPU to hold data
	GPUModelBuffers modelGeometry;
    // PrintModelData(inModel);

    //create vertex buffer
    loader.AddBuffer(vertexBufferSize, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.vertices.data());

	//create index buffer
	loader.AddBuffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.indices.data());

    // create node transform buffer
    loader.AddBuffer(nodeTransformBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.nodeTransforms.data());
    
    // create primitive buffer
    loader.AddBuffer(primitiveBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.primitivePropertiesBufferVec.data());

    // create node primitive pair buffer
    loader.AddBuffer(nodePrimitivePairBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.nodePrimitivePairs.data());

    // create material buffer
    loader.AddBuffer(materialBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.materials.data());

    // create draw command buffer
    loader.AddBuffer(drawCmdBufferVecBufferSize,
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.drawCmdBufferVec.data());


    loader.AddBuffer(instanceTransformBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, (void*) inModel.instanceTransforms.data());

    std::vector<AllocatedBuffer> res = loader.UploadBuffers();
    modelGeometry.vertexBuffer = res[0];
    modelGeometry.indexBuffer = res[1];
    modelGeometry.nodeTransformBuffer = res[2];
    modelGeometry.primitiveBuffer = res[3];
    modelGeometry.nodePrimitivePairBuffer = res[4];
    modelGeometry.materialBuffer = res[5];
    modelGeometry.drawCmdBuffer = res[6];
    modelGeometry.instanceTransformBuffer = res[7];

	return modelGeometry;
}

void Loader::PrintModelData(const LoadedGLTF& modelData){
    fmt::print("Model Data:\n");
    fmt::print("\n\nNodes ------------------------------------\n");
    for (size_t i = 0; i < modelData.nodeTransforms.size(); ++i)
    {
        fmt::print("NodeTransform: {}\n", i);
        for (size_t j = 0; j < 4; ++j)
        {
            fmt::print("({} {} {} {})\n", modelData.nodeTransforms[i][j][0], modelData.nodeTransforms[i][j][1], modelData.nodeTransforms[i][j][2], modelData.nodeTransforms[i][j][3]);
        }
    }
    fmt::print("\n\nPrimitives ------------------------------------\n");
    for (size_t i = 0; i < modelData.primitives.size(); ++i)
    {
        fmt::print("Primitive: {}\n", i);
        fmt::print("VertexStartIdx: {}\n", modelData.primitives[i].vertexStartIdx);
        fmt::print("VertexCount: {}\n", modelData.primitives[i].vertexCount);
        fmt::print("IndexStartIdx: {}\n", modelData.primitives[i].indexStartIdx);
        fmt::print("IndexCount: {}\n", modelData.primitives[i].indexCount);
    }
    fmt::print("\n\nNode Primitive Pairs ------------------------------------\n");
    for (size_t i = 0; i < modelData.nodePrimitivePairs.size(); ++i)
    {
        fmt::print("NodePrimitivePair: {}\n", i);
        fmt::print("NodeIdx: {}\n", modelData.nodePrimitivePairs[i].nodeIdx);
        fmt::print("PrimIdx: {}\n", modelData.nodePrimitivePairs[i].primIdx);
    }

    fmt::print("\n\nDrawCmdBufferVec ------------------------------------\n");
    for (size_t i = 0; i < modelData.drawCmdBufferVec.size(); ++i)
    {
        fmt::print("DrawCmdBuffer: {}\n", i);
        fmt::print("IndexCount: {}\n", modelData.drawCmdBufferVec[i].indexCount);
        fmt::print("InstanceCount: {}\n", modelData.drawCmdBufferVec[i].instanceCount);
        fmt::print("FirstIndex: {}\n", modelData.drawCmdBufferVec[i].firstIndex);
        fmt::print("VertexOffset: {}\n", modelData.drawCmdBufferVec[i].vertexOffset);
        fmt::print("FirstInstance: {}\n", modelData.drawCmdBufferVec[i].firstInstance);
    }
}

void Loader::DestroyModelData(const GPUModelBuffers& modelData, VulkanEngine* engine){
    engine->DestroyBuffer(modelData.vertexBuffer);
    engine->DestroyBuffer(modelData.indexBuffer);
    engine->DestroyBuffer(modelData.nodeTransformBuffer);
    engine->DestroyBuffer(modelData.primitiveBuffer);
    engine->DestroyBuffer(modelData.nodePrimitivePairBuffer);
    engine->DestroyBuffer(modelData.materialBuffer);
    engine->DestroyBuffer(modelData.drawCmdBuffer);
    engine->DestroyBuffer(modelData.instanceTransformBuffer);
    
}

void Loader::Init(VulkanEngine* engine){
    this->engine = engine;
    Clear();
}

void Loader::AddBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, void* data){
    creationParameters.push_back({allocSize, usage, memoryUsage, data});
    totalStagingBufferSize += allocSize;
}

std::vector<AllocatedBuffer> Loader::UploadBuffers(){
    std::vector<AllocatedBuffer> buffers(creationParameters.size());
    
    for (size_t bufferIdx = 0; bufferIdx < creationParameters.size(); ++bufferIdx){
        auto& param = creationParameters[bufferIdx];
        buffers[bufferIdx] = engine->CreateBuffer(param.allocSize, param.usage, param.memoryUsage);
    }

    // create staging buffer to copy all the data
	AllocatedBuffer staging = engine->CreateBuffer(totalStagingBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mappedData;
    vmaMapMemory(engine->_allocator, staging.allocation, &mappedData);

    size_t offset = 0;
    for (size_t bufferIdx = 0; bufferIdx < creationParameters.size(); ++bufferIdx){
        auto& param = creationParameters[bufferIdx];
        memcpy((char*) mappedData + offset, param.data, param.allocSize);
        offset += param.allocSize;
    }

	engine->ImmediateSubmit([&](VkCommandBuffer cmd) {
        size_t offset = 0;
        for (size_t bufferIdx = 0; bufferIdx < creationParameters.size(); ++bufferIdx){
            auto& param = creationParameters[bufferIdx];
            VkBufferCopy copy{ 0 };
            copy.dstOffset = 0;
            copy.srcOffset = offset;
            copy.size = param.allocSize;
            vkCmdCopyBuffer(cmd, staging.buffer, buffers[bufferIdx].buffer, 1, &copy);
            offset += param.allocSize;
        }
	});


    vmaUnmapMemory(engine->_allocator, staging.allocation);
	engine->DestroyBuffer(staging);
    return buffers;
}

void Loader::Clear(){
    creationParameters.clear();
    totalStagingBufferSize = 0;
}

AllocatedImage Loader::LoadCubeMap(const std::vector<std::string> paths, VulkanEngine* engine){
   
    unsigned char *textureData[6];
    int width{ 0 };
    int height{ 0 };
    int nrChannels{ 0 };

    for (int i = 0; i < 6; ++i){
        textureData[i] = stbi_load(paths[i].c_str(), &width, &height, &nrChannels, 4); 
        fmt::print("Loaded Image: {}\n", paths[i]);
    }
    VkExtent3D extent = {width, height, 1};
    AllocatedImage outImage = engine->CreateCubeMap(
        (void**) textureData,
        extent,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false
    );

    for (int i = 0; i < 6; ++i){
        stbi_image_free(textureData[i]);
    }

    return outImage;
}
