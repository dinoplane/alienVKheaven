#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vk_loader.h>


class VulkanEngine;
class Scene {
    public:
        VulkanEngine* _engine;
        std::shared_ptr<LoadedGLTF> _modelData;
        GPUModelBuffers _modelBuffers;

        LightBufferSizeData _lightSizeData;

        AllocatedBuffer _pointLightBuffer;
        AllocatedBuffer _lightSizeDataBuffer;

        std::optional<AllocatedImage> _skyBoxImage;

        void ClearAll();
};
