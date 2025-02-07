#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "vk_mem_alloc.h"


std::optional<std::shared_ptr<LoadedGLTF>> Loader::LoadGltfModel(const std::span<std::string> filePaths){
    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    LoadedGLTF& loadedModel = *scene.get();
    fastgltf::Parser parser {};
    for (auto& filePath : filePaths){
     
        fmt::print("Loading GLTF: {}", filePath);

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

        loadedModel.meshes.resize(loadedModel.meshes.size() + gltfAsset.meshes.size());
        for ( size_t meshIdx = 0; meshIdx < gltfAsset.meshes.size(); ++meshIdx ){
            const fastgltf::Mesh& gltfMesh = gltfAsset.meshes[meshIdx];
            LoadedMesh& loadedMesh = loadedModel.meshes[meshIdx];
            
            if (!Loader::LoadGltfMesh(gltfAsset, gltfMesh, &loadedModel, &loadedMesh, &loadedModel.vertices, &loadedModel.indices, &loadedModel.primitives)){
                assert(false);
            }
        }

        // Create scene graph by iterating through the nodes.
        size_t initNodesSize = loadedModel.nodes.size();
        // loadedModel.nodes.resize(initNodesSize + gltfAsset.nodes.size());
        fastgltf::iterateSceneNodes(gltfAsset, 0u, fastgltf::math::fmat4x4(),
                                    [&](const fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {
                                        
            glm::mat4 worldTransform = glm::make_mat4(matrix.data());
            glm::mat4 localTransform = glm::make_mat4(fastgltf::getTransformMatrix(node).data());
            if (node.meshIndex.has_value()) {
                loadedModel.nodes.push_back( Node{worldTransform, localTransform, static_cast<uint32_t>(*node.meshIndex), UINT32_MAX} );
            } else {
                loadedModel.nodes.push_back( Node{worldTransform, localTransform, UINT32_MAX, UINT32_MAX} );
            
                // loadedModel.nodePropertiesBufferVec.push_back( { modelFromNodeMat } );
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

        size_t initTopNodesSize = loadedModel.topNodes.size();
        for (size_t nodeIdx = initNodesSize; nodeIdx < loadedModel.nodes.size(); ++nodeIdx) {
            Node& node = loadedModel.nodes[nodeIdx];
            if (node.parentIdx == UINT32_MAX) {
                loadedModel.topNodes.push_back(nodeIdx);
                loadedModel.refreshTransform(glm::mat4(1.0f), nodeIdx);
            }
        }


        size_t initNodePrimitivePairSize = loadedModel.nodePrimitivePairs.size();
        size_t initDrawCmdBufferVecSize = loadedModel.drawCmdBufferVec.size();
        // Generate draw calls by iterating through nodes across primitives.
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
                    1,						// instanceCount
                    prim.indexStartIdx,		// firstIndex
                    prim.vertexStartIdx,	// baseVertex TODO: WHY IS THIS AN INT???? 
                    0						// baseInstance
                } );
            }
        } 

        loadedModel.modelDataVec.push_back( ModelData{
            static_cast<uint32_t>(initMeshesSize), static_cast<uint32_t>(gltfAsset.meshes.size()),
            static_cast<uint32_t>(initNodesSize), static_cast<uint32_t>(gltfAsset.nodes.size()),
            static_cast<uint32_t>(initPrimitiveSize), static_cast<uint32_t>(loadedModel.primitives.size() - initPrimitiveSize),
            static_cast<uint32_t>(initNodePrimitivePairSize), static_cast<uint32_t>(loadedModel.nodePrimitivePairs.size() - initNodePrimitivePairSize),
            static_cast<uint32_t>(initTopNodesSize), static_cast<uint32_t>(loadedModel.topNodes.size() - initTopNodesSize),
            static_cast<uint32_t>(initDrawCmdBufferVecSize), static_cast<uint32_t>(loadedModel.drawCmdBufferVec.size() - initDrawCmdBufferVecSize)
        } );

        // Generate buffers and send them to the device
        // ;


    }

    return scene;    
}


bool LoadGltfMesh(const fastgltf::Asset& gltfAsset, const fastgltf::Mesh& gltfMesh,
                        LoadedGLTF* outModel, LoadedMesh* outMesh, 
                        std::vector<Vertex>* vertices, std::vector<uint32_t>* indices,
                        std::vector<LoadedPrimitive>* primitives){
    
    size_t initPrimitivesSize = primitives->size();
	outMesh->primitiveStartIdx = initPrimitivesSize;
	outMesh->primitiveCount = static_cast<uint32_t>(gltfMesh.primitives.size());
    primitives->resize(initPrimitivesSize + gltfMesh.primitives.size());

    for (auto it = gltfMesh.primitives.begin(); it != gltfMesh.primitives.end(); ++it) {
		LoadedPrimitive outPrimitive;
        
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

		// auto& draw = outPrimitive.draw;
		// draw.instanceCount = 1;
        // draw.baseInstance = 0;
        // draw.baseVertex = 0;
		// draw.firstIndex = 0;

        auto& indexAccessor = gltfAsset.accessors[it->indicesAccessor.value()];
		// draw.count = static_cast<std::uint32_t>(indexAccessor.count);

		size_t initIndicesSize = indices->size();
		outPrimitive.indexStartIdx = initIndicesSize;
		outPrimitive.indexCount = indexAccessor.count;
		indices->resize( initIndicesSize + indexAccessor.count );

		fastgltf::iterateAccessorWithIndex<uint32_t>(gltfAsset, indexAccessor, [&](uint32_t indiceIdx, std::size_t idx) {
				( *indices )[ initIndicesSize + idx ] = indiceIdx;
		});


		// std::size_t baseColorTexcoordIndex = 0;
        // Get the output primitive
        // auto index = std::distance(gltfMesh.primitives.begin(), it);
        /*
		model->primitivePropertiesBufferVec.push_back( { 0, 0 } ); // I could also pack both into a value
        if (it->materialIndex.has_value()) {
            outPrimitive.materialUniformsIndex = it->materialIndex.value() + 1; // Adjust for default material
			model->primitivePropertiesBufferVec.back().materialIdx = it->materialIndex.value() + 1;

            auto& material = asset.materials[it->materialIndex.value()];

			auto& baseColorTexture = material.pbrData.baseColorTexture;
            if (baseColorTexture.has_value()) {
                auto& texture = asset.textures[baseColorTexture->textureIndex];
				if (!texture.imageIndex.has_value())
					return false;
                outPrimitive.albedoTexture = model->textures[texture.imageIndex.value()].texture;

				model->primitivePropertiesBufferVec.back().textureIdx = texture.imageIndex.value();
				// fmt::print("Albedo Loaded {}\n", outPrimitive.albedoTexture);
				if (baseColorTexture->transform && baseColorTexture->transform->texCoordIndex.has_value()) {
					baseColorTexcoordIndex = baseColorTexture->transform->texCoordIndex.value();
				} else {
					baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
				}
            }
        } else {
			outPrimitive.materialUniformsIndex = 0;
		}
        */

		primitives->push_back(outPrimitive);
    }

    return true;
}

void LoadedGLTF::clearAll(){
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
}

GPUModelBuffers Loader::LoadGeometryFromGLTF(LoadedGLTF& inModel, VulkanEngine* engine){
    // I feel like uploading buffers is a common operation... we could abstract this code into a helper function/class
    

    const size_t vertexBufferSize = inModel.vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = inModel.indices.size() * sizeof(uint32_t);
    const size_t nodeTransformBufferSize = inModel.nodeTransforms.size() * sizeof(glm::mat4);
    const size_t primitiveBufferSize = inModel.primitives.size() * sizeof(LoadedPrimitive);
    const size_t nodePrimitivePairBufferSize = inModel.nodePrimitivePairs.size() * sizeof(NodePrimitivePair);
    const size_t drawCmdBufferVecBufferSize = inModel.drawCmdBufferVec.size() * sizeof(VkDrawIndexedIndirectCommand);

    // create buffer on GPU to hold data
	GPUModelBuffers modelGeometry;

	//create vertex buffer
	modelGeometry.vertexBuffer = engine->create_buffer(vertexBufferSize, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	//find the adress of the vertex buffer
	VkBufferDeviceAddressInfo deviceVtxAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = modelGeometry.vertexBuffer.buffer };
	modelGeometry.vertexBufferAddress = vkGetBufferDeviceAddress(engine->_device, &deviceVtxAddressInfo);

	//create index buffer
	modelGeometry.indexBuffer = engine->create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);

	//find the adress of the index buffer
	VkBufferDeviceAddressInfo deviceIdxAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,.buffer = modelGeometry.indexBuffer.buffer };
	modelGeometry.indexBufferAddress = vkGetBufferDeviceAddress(engine->_device, &deviceIdxAddressInfo);


    // create node transform buffer
    modelGeometry.nodeTransformBuffer = engine->create_buffer(nodeTransformBufferSize,
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    
    // create primitive buffer
    modelGeometry.primitiveBuffer = engine->create_buffer(primitiveBufferSize,
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // create node primitive pair buffer
    modelGeometry.nodePrimitivePairBuffer = engine->create_buffer(nodePrimitivePairBufferSize,
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // create draw command buffer
    modelGeometry.drawCmdBuffer = engine->create_buffer(drawCmdBufferVecBufferSize,
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT 
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


    // create staging buffer to copy all the data
	AllocatedBuffer staging = engine->create_buffer(vertexBufferSize + indexBufferSize + nodeTransformBufferSize + primitiveBufferSize + nodePrimitivePairBufferSize + drawCmdBufferVecBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* mappedData;
    vmaMapMemory(engine->_allocator, staging.allocation, &mappedData);


	// copy vertex buffer
	memcpy(mappedData, inModel.vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)mappedData + vertexBufferSize, inModel.indices.data(), indexBufferSize);
    // copy node transform buffer
    memcpy((char*)mappedData + vertexBufferSize + indexBufferSize, inModel.nodeTransforms.data(), nodeTransformBufferSize);
    // copy primitive buffer
    memcpy((char*)mappedData + vertexBufferSize + indexBufferSize + nodeTransformBufferSize, inModel.primitives.data(), primitiveBufferSize);
    // copy node primitive pair buffer
    memcpy((char*)mappedData + vertexBufferSize + indexBufferSize + nodeTransformBufferSize + primitiveBufferSize, inModel.nodePrimitivePairs.data(), nodePrimitivePairBufferSize);
    // copy draw command buffer
    memcpy((char*)mappedData + vertexBufferSize + indexBufferSize + nodeTransformBufferSize + primitiveBufferSize + nodePrimitivePairBufferSize, inModel.drawCmdBufferVec.data(), drawCmdBufferVecBufferSize);

	engine->immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.indexBuffer.buffer, 1, &indexCopy);

        VkBufferCopy nodeTransformCopy{ 0 };
        nodeTransformCopy.dstOffset = 0;
        nodeTransformCopy.srcOffset = vertexBufferSize + indexBufferSize;
        nodeTransformCopy.size = nodeTransformBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.nodeTransformBuffer.buffer, 1, &nodeTransformCopy);

        VkBufferCopy primitiveCopy{ 0 };
        primitiveCopy.dstOffset = 0;
        primitiveCopy.srcOffset = vertexBufferSize + indexBufferSize + nodeTransformBufferSize;
        primitiveCopy.size = primitiveBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.primitiveBuffer.buffer, 1, &primitiveCopy);

        VkBufferCopy nodePrimitivePairCopy{ 0 };
        nodePrimitivePairCopy.dstOffset = 0;
        nodePrimitivePairCopy.srcOffset = vertexBufferSize + indexBufferSize + nodeTransformBufferSize  + primitiveBufferSize;
        nodePrimitivePairCopy.size = nodePrimitivePairBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.nodePrimitivePairBuffer.buffer, 1, &nodePrimitivePairCopy);

        VkBufferCopy drawCmdBufferCopy{ 0 };
        drawCmdBufferCopy.dstOffset = 0;
        drawCmdBufferCopy.srcOffset = vertexBufferSize + indexBufferSize + nodeTransformBufferSize + primitiveBufferSize + nodePrimitivePairBufferSize;
        drawCmdBufferCopy.size = drawCmdBufferVecBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, modelGeometry.drawCmdBuffer.buffer, 1, &drawCmdBufferCopy);
	});

	engine->destroy_buffer(staging);

	return modelGeometry;
}

