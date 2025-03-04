#include <vk_scenedata.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <glm/gtx/string_cast.hpp>

void PrintSceneData(const SceneData& sceneData)
{
    for ( const EntityData& entitydata : sceneData.entitiesData){
        fmt::print("------------------------------------\n");
        fmt::print("Entity: {}\n", entitydata.className);
        fmt::print("Transform: {}\n", glm::to_string(entitydata.transform.GetModelMatrix()));
        for (const auto& kvp : entitydata.kvps){
            fmt::print("{}: {}\n", kvp.first, kvp.second);
        }

    }
}