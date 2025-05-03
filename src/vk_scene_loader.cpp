
#include <vk_scene_loader.h>
#include <vk_loader.h>
#include <vk_scenedata.h>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include <vk_engine.h>

#include "stb_image.h"
static glm::vec3 ParseVec3(const std::string& vec3Str)
{
    glm::vec3 vec;
    std::istringstream iss(vec3Str);
    iss >> vec.x >> vec.y >> vec.z;
    return vec;
}


static constexpr uint64_t hash(std::string_view str) {
    uint64_t hash = 0;
    for (char c : str) {
        hash = (hash * 131) + c;
    }
    return hash;
}

// https://medium.com/@ryan_forrester_/using-switch-statements-with-strings-in-c-a-complete-guide-efa12f64a59d cool beans
static constexpr uint64_t operator"" _hash(const char* str, size_t len) {
    return hash(std::string_view(str, len));
}

void SceneLoader::LoadScene(const SceneData& sceneData, Scene* scene, VulkanEngine* engine)
{
    if (scene == nullptr)
    {
        fmt::print("ScenePointer is nullptr\n");
        assert(false);
    }

    // Add entities as instances. 
    std::unordered_map<std::string, std::vector<glm::mat4>> modelToInstanceModelMatrices;
    std::vector<PointLightData> pointLights;
    std::vector<glm::mat4> lightTransforms;
    std::vector<VkDrawIndexedIndirectCommand> debugDrawCmdBufferVec;

    for (const EntityData& entityData : sceneData.entitiesData)
    {
        std::string classname = entityData.className;
        
        switch (hash(classname)) {
            case "model"_hash:
            {
                modelToInstanceModelMatrices[entityData.kvps.at("path")].push_back(entityData.transform.GetModelMatrix());
            } break;

            case "skybox"_hash:
            {
                const char* imageKeys[] = {
                    "front", "back", "top", "bottom", "left", "right"
                };
                std::vector<std::string> imagePaths;
                for (int i = 0; i < 6; ++i){
                    const std::string key = imageKeys[i];
                    const std::string path = entityData.kvps.at(key);
                    imagePaths.push_back(path);
                }
                scene->_skyBoxImage = Loader::LoadCubeMap(imagePaths, engine);
            } break;

            case "point_light"_hash:
            {
                PointLightData pointLight;
                pointLight.position = entityData.transform.originVec;
                pointLight.color = entityData.lightData.color;
                pointLight.intensity = entityData.lightData.intensity;
                pointLight.radius = entityData.lightData.radius;
                pointLights.push_back(pointLight);
                lightTransforms.push_back(entityData.transform.GetModelMatrix());
                lightTransforms.push_back(entityData.transform.translation * glm::scale(glm::vec3(0.5f))); // for debug draw
            } break;

            default:
                fmt::print("Unknown class: {} ", classname);
        }
    }
    scene->_engine = engine;
    scene->_modelData = Loader::LoadGltfModel(engine, modelToInstanceModelMatrices).value();
    scene->_modelBuffers = Loader::LoadGeometryFromGLTF(*scene->_modelData.get() , engine);

    SceneLoader::LoadPointLights(engine, scene, &pointLights, &lightTransforms, &debugDrawCmdBufferVec);
}

void SceneLoader::LoadPointLights(VulkanEngine* engine, Scene* scene,
    std::vector<PointLightData>* pointLights,
    std::vector<glm::mat4>* lightTransforms,
    std::vector<VkDrawIndexedIndirectCommand>* debugDrawCmdBufferVec){

    scene->_lightSizeData.numPointLights = pointLights->size();


    // Create debug draw commands
    struct VkDrawIndexedIndirectCommand debugPointLightCmd;
    debugPointLightCmd.firstInstance = 0;
    debugPointLightCmd.instanceCount = pointLights->size() * 2;
    debugPointLightCmd.firstIndex = 0;
    debugPointLightCmd.vertexOffset = 0;
    debugPointLightCmd.indexCount = engine->_sphereIndexCount; // This for a sphere
    debugDrawCmdBufferVec->push_back(debugPointLightCmd);

    scene->_debugDrawCount = debugDrawCmdBufferVec->size();

    if (pointLights->size() == 0) {
        pointLights->push_back(PointLightData{
            .position = glm::vec3(0.0f),
            .radius = 1.0f,
            .color = glm::vec3(1.0f),
            .intensity = 1.0f
        });
        lightTransforms->push_back(glm::mat4(1.0f));
        scene->_hasPointLights = false;
    } else scene->_hasPointLights = true;

    Loader loader;
    loader.Init(engine);

    // Create buffers for lights and debug draw commands
    loader.AddBuffer(sizeof(PointLightData) * pointLights->size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, pointLights->data());
    loader.AddBuffer(sizeof(LightBufferSizeData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &scene->_lightSizeData);
    loader.AddBuffer(sizeof(glm::mat4) * lightTransforms->size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, lightTransforms->data());
    loader.AddBuffer(sizeof(VkDrawIndexedIndirectCommand) * debugDrawCmdBufferVec->size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, debugDrawCmdBufferVec->data());

    // For omni directional shadows, we know how many lights we have, so we know how many textures to create
    // To keep things flexible, we will not use cube maps for point lights, but rather use 6 512 x 512 textures

    const uint32_t shadowMapWidth = SHADOW_MAP_WIDTH;
    scene->_shadowMapCount = pointLights->size();
    scene->_shadowMapExtent = VkExtent2D{ shadowMapWidth, shadowMapWidth };
    scene->_shadowMapLayers = 6; // TODO: use a layered image instead of 6 images

    for (int i = 0; i < scene->_shadowMapCount; ++i) { // TODO: use a layered image instead of 6 images
        AllocatedImage shadowMap = engine->CreateImage(
                                                    VkExtent3D{ shadowMapWidth, shadowMapWidth, 1 }, 
                                                    VK_FORMAT_D32_SFLOAT, 
                                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 
                                                    false, 
                                                    scene->_shadowMapLayers);
        scene->_shadowMapBuffer.push_back(shadowMap);
    }

    std::vector<AllocatedBuffer> buffers = loader.UploadBuffers();
    scene->_pointLightBuffer = buffers[0];
    scene->_lightSizeDataBuffer = buffers[1];
    scene->_lightTransformBuffer = buffers[2];
    scene->_debugDrawCmdBuffer = buffers[3];



}


void SceneLoader::LoadSceneData(const std::string& scenePath, SceneData* sceneData)
{
    if (sceneData == nullptr)
    {
        fmt::print("SceneDataPointer is nullptr\n");
        assert(false);
    }
    // Load scene data from file
    // Here I could do a couple of things:
    // mmap the file and read it in chunks  (oo this is kinda fun to do later)
    // use istream 

    // For now, I'll just use ifstream
    std::ifstream sceneFile(scenePath);
    if (!sceneFile.is_open())
    {
        sceneData->errFlag = true;
        fmt::print("Failed to open scene file: {}\n", scenePath);
        assert(false);
    }

    std::string line;
    EntityData data;
    while (std::getline(sceneFile, line))
    {
        if (line.empty())
        {
            continue;
        } else if (line[0] == '{')
        {
            // Start of a new entity
            data = EntityData();
            continue;

        } else if (line[0] == '}')
        {
            // End of entity
            sceneData->entitiesData.push_back(data);
            continue;
        } else {
            std::istringstream iss(line);
            std::string key;
            std::string value;
            while (iss >> std::quoted(key))
            {
                // TODO These should be done by type not key name
                iss >> std::quoted(value);
                switch (hash(key)) {
                    case "classname"_hash:
                    {
                        data.className = value;
                    } break;
                    case "origin"_hash:
                    {
                        data.transform.SetPosition(ParseVec3(value));
                    } break;
                    case "angles"_hash:
                    {
                        data.transform.SetRotation(ParseVec3(value));
                    } break;
                    case "scale"_hash:
                    {
                        data.transform.SetScale(ParseVec3(value));
                    } break;
                    case "radius"_hash:
                    {
                        data.lightData.radius = std::stof(value);
                        data.transform.SetScale(glm::vec3(data.lightData.radius));
                    } break;
                    case "color"_hash:
                    {
                        data.lightData.color = ParseVec3(value);
                    } break;
                    case "intensity"_hash:
                    {
                        data.lightData.intensity = std::stof(value);
                    } break;
                    case "direction"_hash:
                    {
                        data.lightData.direction = ParseVec3(value);
                    } break;
                }
                data.kvps[key] = value;
            // if (token == "entity")
            // {
            //     EntityData entityData;
            //     std::string meshPath;
            //     iss >> meshPath;
            //     entityData.meshData = MeshData::CreateCube();
            //     entityData.transform = Transform();
            //     sceneData->entitiesData.push_back(entityData);
            // }
            // else if (token == "camera")
            // {
            //     Camera camera;
            //     float fov, aspect, near, far;
            //     glm::vec3 pos, up, front;
            //     iss >> fov >> aspect >> near >> far;
            //     iss >> pos.x >> pos.y >> pos.z;
            //     iss >> up.x >> up.y >> up.z;
            //     iss >> front.x >> front.y >> front .z;
            //     camera = Camera(fov, aspect, near, far, pos, up, front);
            //     sceneData->cameraData.push_back(camera);
            // }
            }

    }
    }
    
}
