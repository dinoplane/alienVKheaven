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


        bool _hasPointLights{false};
        AllocatedBuffer _pointLightBuffer;
        AllocatedBuffer _lightSizeDataBuffer;
        AllocatedBuffer _lightTransformBuffer;
        std::vector<AllocatedImage> _shadowMapBuffer; // TODO: could also be just one atlas highkey...
        uint32_t _shadowMapCount{0};
        uint32_t _shadowMapLayers{0};
        VkExtent2D _shadowMapExtent{0, 0};
        
        AllocatedBuffer _debugDrawCmdBuffer;
        uint32_t _debugDrawCount;

        std::optional<AllocatedImage> _skyBoxImage;



        void ClearAll();
};
