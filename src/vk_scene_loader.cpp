
#include <vk_scene_loader.h>
#include <vk_loader.h>
#include <vk_scenedata.h>
#include <sstream>
#include <string_view>
#include <unordered_set>

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
                scene->skyBoxImages = Loader::LoadCubeMap(imagePaths, engine);
            } break;
            case "point_light"_hash:
            {
                PointLightData pointLight;
                pointLight.position = entityData.transform.originVec;
                pointLight.color = entityData.lightData.color;
                pointLight.intensity = entityData.lightData.intensity;
                pointLight.radius = entityData.lightData.radius;
                pointLights.push_back(pointLight);
            } break;

            // case "load"_hash:
            //     std::cout << "Loading data..." << std::endl;
            //     break;
            // case "delete"_hash:
            //     std::cout << "Deleting data..." << std::endl;
            //     break;
            default:
                fmt::print("Unknown class: {} ", classname);
        }
    }
    scene->engine = engine;
    scene->modelData = Loader::LoadGltfModel(engine, modelToInstanceModelMatrices).value();
    scene->modelBuffers = Loader::LoadGeometryFromGLTF(*scene->modelData.get() , engine);

    scene->lightSizeData.numPointLights = pointLights.size();

    Loader loader;
    loader.Init(engine);
    loader.AddBuffer(sizeof(PointLightData) * pointLights.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, pointLights.data());

    loader.AddBuffer(sizeof(LightBufferSizeData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, &scene->lightSizeData);

    std::vector<AllocatedBuffer> buffers = loader.UploadBuffers();
    scene->pointLightBuffer = buffers[0];
    scene->lightSizeDataBuffer = buffers[1];
    
    // scene->pointLightBuffer = engine->create_buffer(sizeof(PointLightData) * pointLights.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
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
