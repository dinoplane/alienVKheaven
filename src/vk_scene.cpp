#include <vk_scene.h>
#include "vk_engine.h"

void Scene::ClearAll() {
    if (_modelData) {
        for (const AllocatedImage& img : _modelData->images) {
            _engine->DestroyImage(img);
        }
        Loader::DestroyModelData(_modelBuffers, _engine);
        _modelData->ClearAll();
    }
    if (_skyBoxImage.has_value()) {
        _engine->DestroyImage(*_skyBoxImage);
    }

    _engine->DestroyBuffer(_pointLightBuffer);
    _engine->DestroyBuffer(_lightSizeDataBuffer);
}