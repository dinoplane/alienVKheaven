#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <vk_scene.h>
#include <vk_scenedata.h>


class SceneLoader {
    public: 
    static void LoadScene(const SceneData& sceneData, Scene* scene, VulkanEngine* engine);
    static void LoadSceneData(const std::string& scenePath, SceneData* sceneData);
    // static void LoadCameraSettings(const std::string& cameraSettingsPath, std::vector<Camera>* camerasm, std::vector<Primitive>* debugMeshes);
    // static void SaveCameraSettings(const std::string& cameraSettingsPath, const std::vector<Camera>& cameras);
};

