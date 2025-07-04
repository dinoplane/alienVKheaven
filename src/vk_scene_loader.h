#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <vk_scene.h>
#include <vk_scenedata.h>
#include <vk_constants.h>
#include <camera.h>

class SceneLoader {
    public: 
    static void LoadPointLights(VulkanEngine* engine, Scene* scene,
        std::vector<PointLightData>* pointLights,
        std::vector<glm::mat4>* lightTransforms,
        std::vector<VkDrawIndexedIndirectCommand>* debugDrawCmdBufferVec
    );
    static void LoadScene(const SceneData& sceneData, Scene* scene, VulkanEngine* engine);
    static void LoadSceneData(const std::string& scenePath, SceneData* sceneData);
    // static void LoadCameraSettings(const std::string& cameraSettingsPath, std::vector<Camera>* camerasm, std::vector<Primitive>* debugMeshes);
    // static void SaveCameraSettings(const std::string& cameraSettingsPath, const std::vector<Camera>& cameras);
    static void SaveCameraSettings(const std::string& cameraSettingsPath, const Camera& camera);
    static Camera LoadCameraSettings(const std::string& cameraSettingsPath);

};

