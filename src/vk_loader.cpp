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

std::optional<std::shared_ptr<LoadedGLTF>> Loader::LoadGltfModel(std::string_view filePath){
    fmt::print("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    // scene->creator = engine;
    LoadedGLTF& loadedModel = *scene.get();

    fastgltf::Parser parser {};

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

    loadedModel.vertices.clear();
    loadedModel.indices.clear();

    loadedModel.meshes.resize(gltfAsset.meshes.size());
	for ( size_t meshIdx = 0; meshIdx < gltfAsset.meshes.size(); ++meshIdx ){
		const fastgltf::Mesh& gltfMesh = gltfAsset.meshes[meshIdx];
		LoadedMesh& loadedMesh = loadedModel.meshes[meshIdx];
        
        if (!Loader::LoadGltfMesh(gltfAsset, gltfMesh, &loadedModel, &loadedMesh, &loadedModel.vertices, &loadedModel.indices)){
			assert(false);
			return;
		}
	}

    // Create scene graph by iterating through the nodes.
    loadedModel.nodes.resize(gltfAsset.nodes.size());
	fastgltf::iterateSceneNodes(gltfAsset, 0u, fastgltf::math::fmat4x4(),
								[&](const fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {
		if (node.meshIndex.has_value()) {
			glm::mat4 modelFromNodeMat = glm::make_mat4(matrix.data());
			loadedModel.nodes.push_back( Node{modelFromNodeMat, static_cast<uint32_t>(*node.meshIndex) } );
            // for local transform, use fastgltf::getTransformMatrix(node)
            
			// loadedModel.nodePropertiesBufferVec.push_back( { modelFromNodeMat } );
		}
	});


    // Generate draw calls by iterating through nodes across primitives.
    

    return scene;    
}


bool LoadGltfMesh(const fastgltf::Asset& gltfAsset, const fastgltf::Mesh& gltfMesh,
                        LoadedGLTF* outModel, LoadedMesh* outMesh, 
                        std::vector<Vertex>* vertices, std::vector<uint32_t>* indices){

	outMesh->primitiveStartIdx = static_cast<uint32_t>(outModel->primitives.size());
	outMesh->primitiveCount = static_cast<uint32_t>(gltfMesh.primitives.size());
    outModel->primitives.resize(outModel->primitives.size() + gltfMesh.primitives.size());

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

		outModel->primitives.push_back(outPrimitive);
    }

    return true;
}



