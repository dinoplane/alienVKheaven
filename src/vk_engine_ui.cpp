
#include <vk_engine.h>
#include <fmt/core.h>

#include <nfd.h>


void VulkanEngineUI::RenderVulkanEngineUI(VulkanEngineUIState* engineUIState, VulkanEngine* engine){
    ImGui::Text(engineUIState->status.c_str());
    if (ImGui::Button("Load Scene")) {
        // Open a dialog
        nfdchar_t *outPath = NULL;
        nfdresult_t result = NFD_OpenDialog( "scn", NULL, &outPath );
            
        if ( result == NFD_OKAY ) {
            engine->LoadScene(outPath);
            (*engineUIState).loadedPath = outPath;
            (*engineUIState).status = fmt::format("Loaded Scene: {}\n", outPath);
            free(outPath);
        }
        else if ( result == NFD_CANCEL ) {
            (*engineUIState).status = "Loaded Scene: Cancelled\n";
        }
        else {
            (*engineUIState).status = fmt::format("Error: %s\n", NFD_GetError() );
        }
    }
    if (ImGui::Button("Reload Scene")) {
        // engine->UnloadScene();
        if (engine->isSceneLoaded){
            engine->sceneLoadFlag = VulkanEngine::SceneLoadFlag::SCENE_LOAD_FLAG_RELOAD;
            (*engineUIState).status = fmt::format("Reloaded Scene: {}\n", (*engineUIState).loadedPath);    
        }
    }
    if (ImGui::Button("Clear")) {
        // engine->UnloadScene();
        engine->scene_clear_requested = true;
        (*engineUIState).status = "Loaded Scene: None\n";
    }
}