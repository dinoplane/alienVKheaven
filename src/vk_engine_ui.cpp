
#include <vk_engine.h>
#include <fmt/core.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <tracy/Tracy.hpp>

#include <nfd.h>

void VulkanEngineUI::RenderVulkanEngineUI(VulkanEngineUIState* engineUIState, VulkanEngine* engine){
    ImGui::SeparatorText("Scene");
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

void VulkanEngineUI::RenderGlobalParamUI(VulkanEngineUIState* engineUIState, VulkanEngine* engine){
    

    ImGui::SeparatorText("Global Parameters");
    ImGui::Text("Light Direction");
    if (ImGui::SliderFloat("Theta", &engineUIState->theta, 0.0f, 360.0f)){
        // fmt::print("Theta: {}\n", engineUIState->theta);
        
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NavNoCaptureKeyboard; // Enable Keyboard Controls
        engine->sceneUniformData.lightDirection.x = -cos(glm::radians(engineUIState->theta)) * cos(glm::radians(engineUIState->phi));
        engine->sceneUniformData.lightDirection.z = -sin(glm::radians(engineUIState->theta)) * cos(glm::radians(engineUIState->phi));
        engine->sceneUniformData.lightDirection.y = -sin(glm::radians(engineUIState->phi));
    }
    if (ImGui::SliderFloat("Phi", &engineUIState->phi, 0.0f, 360.0f)){
        // fmt::print("Phi: {}\n", engineUIState->phi);
        engine->sceneUniformData.lightDirection.x = -cos(glm::radians(engineUIState->theta)) * cos(glm::radians(engineUIState->phi));
        engine->sceneUniformData.lightDirection.z = -sin(glm::radians(engineUIState->theta)) * cos(glm::radians(engineUIState->phi));
        engine->sceneUniformData.lightDirection.y = -sin(glm::radians(engineUIState->phi));
    }
    ImGui::Text("%.2f, %.2f, %.2f", engine->sceneUniformData.lightDirection.x, engine->sceneUniformData.lightDirection.y, engine->sceneUniformData.lightDirection.z);
    ImGui::Separator();

    ImGui::SeparatorText("Camera Position");
    ImGui::Text("%.2f, %.2f, %.2f", engine->_camera.position.x, engine->_camera.position.y, engine->_camera.position.z);

    ImGui::SeparatorText("Camera Front");
    ImGui::Text("%.2f, %.2f, %.2f", engine->_camera.front.x, engine->_camera.front.y, engine->_camera.front.z);

    ImGui::SeparatorText("Frame Time");
    ImGui::Text("%.5f", engine->_deltaTime);


    TracyPlot("Frame Time", engine->_deltaTime);

    ImGui::SeparatorText("Frame Per Second (FPS)");
    ImGui::Text("%.2f", 1.0f / engine->_deltaTime);
    


    ImGui::SeparatorText("Rendering Flags");
    ImGui::Checkbox("Render debug volumes", &engine->_showDebugVolumes);    
    ImGui::Checkbox("Enable shadows", &engine->_enableShadows);
    if (engine->_enableShadows){
        engine->sceneUniformData.enableShadows = 1;
    } else {
        engine->sceneUniformData.enableShadows = 0;
    }
    
}
