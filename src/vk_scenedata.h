#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <vk_types.h>

#include <camera.h>

struct EntityData {
    
    std::string className;
    Transform transform;
    bool isInstance;
    LightSceneData lightData;
    std::unordered_map<std::string, std::string> kvps;
};

struct SceneData {
    // std::set<std::string> entityKeys; // so i could conserve space here but uh... save for another day 
    std::vector<EntityData> entitiesData;
    // std::vector<Light> lightsData;

    std::string skyboxPath;

    // std::vector<Camera> cameraData;

    bool errFlag = false;

    // at this point its becoming an engine LMAO
    // static SceneData GenerateSceneFromConfig(const std::string& configPath);
    // If I process the file multithreadedly, TECHNICALLY, i can guarantee a random bvh insertion...
    // Wait thats lk troll and funny.
};


void PrintSceneData(const SceneData& sceneData);
