#include <vk_scene.h>
#include "vk_engine.h"

void Scene::ClearAll() {
    if (modelData) {
        for (const AllocatedImage& img : modelData->images) {
            engine->destroy_image(img);
        }
        Loader::DestroyModelData(modelBuffers, engine);
        modelData->ClearAll();
    }
}