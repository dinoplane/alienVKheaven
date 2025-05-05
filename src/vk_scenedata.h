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
    std::vector<EntityData> entitiesData;
    std::string skyboxPath;
    Camera cameraData;
    bool errFlag = false;
};


void PrintSceneData(const SceneData& sceneData);
