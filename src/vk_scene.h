#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vk_loader.h>


class VulkanEngine;
class Scene {
    public:
        VulkanEngine* engine;
        std::shared_ptr<LoadedGLTF> modelData;
        GPUModelBuffers modelBuffers;

        std::optional<AllocatedImage> skyBoxImages;

        void ClearAll();
};
