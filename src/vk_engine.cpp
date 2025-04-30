#define _CRTDBG_MAP_ALLOC
#include "vk_engine.h"


#include <thread>
#include <chrono>

#include <fmt/core.h>
#include <fmt/printf.h>
#include "vk_scene_loader.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"


VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::Init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

	InitVulkan();

	InitSwapchain();

	InitCommands();

	InitSyncStructures();

	InitDescriptors(); // I don't like this

	InitPipelines(); // I also don't like this

	InitImgui();

    InitDefaultData();

    // everything went fine
    _lastFrame = 0;
    _lastTimePoint = std::chrono::high_resolution_clock::now();
    _frameTimer = 0;
    _isInitialized = true;
}

void VulkanEngine::Cleanup()
{	
	if (_isInitialized) {
		
		//make sure the gpu has stopped doing its things
		vkDeviceWaitIdle(_device);
		
		for (int i = 0; i < FRAME_OVERLAP; i++) {

			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

			//destroy sync objects
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);

			_frames[i]._deletionQueue.flush();
		}

		//for (auto& mesh : testMeshes) {
		//	DestroyBuffer(mesh->meshBuffers.indexBuffer);
		//	DestroyBuffer(mesh->meshBuffers.vertexBuffer);
		//}
		UnloadScene();

		_mainDeletionQueue.flush();

		DestroySwapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);

		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::Draw()
{
//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));

	get_current_frame()._deletionQueue.flush();
	// get_current_frame()._frameDescriptors.clear_pools(_device);

	//request image from the swapchain
	uint32_t swapchainImageIndex;

	VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	_drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
	_drawExtent.width= std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//write the buffer
	void* sceneMappedPtr;
	vmaMapMemory(_allocator, get_current_frame()._gpuSceneDataBuffer.allocation, &sceneMappedPtr);
	memcpy(sceneMappedPtr, &sceneUniformData, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator,  get_current_frame()._gpuSceneDataBuffer.allocation);

	VkBufferCopy copy{};
	copy.dstOffset = 0;
	copy.srcOffset = 0;
	copy.size = sizeof(GPUSceneData);

	vkCmdCopyBuffer(cmd, get_current_frame()._gpuSceneDataBuffer.buffer, _gpuSceneDataBuffer.buffer, 1, &copy);


	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	VkClearColorValue clearValue;
	// float flash = std::abs(std::sin(_frameNumber / 120.f));
	clearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
	



	if (isSceneLoaded){
		if (scene->_skyBoxImage.has_value()) {
			VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			DrawSkyBoxPass(cmd);
			VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		}
		
		// for (uint32_t i = 0; i < scene->_shadowMapCount; i++) {
		// 	VkUtil::TransitionImage(cmd, scene->_shadowMapBuffer[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			
		// 	VkClearDepthStencilValue clearDepthValue;
		// 	// float flash = std::abs(std::sin(_frameNumber / 120.f));
		// 	clearValue = { 1.0f, 0.0f };
		
		// 	VkImageSubresourceRange clearDepthRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
			
		// 	vkCmdClearDepthStencilImage(cmd, scene->_shadowMapBuffer[i].image, VK_IMAGE_LAYOUT_GENERAL, &clearDepthValue, 1, &clearDepthRange);
		// }

		VkUtil::TransitionImage(cmd, _positionImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		VkUtil::TransitionImage(cmd, _normalImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		VkUtil::TransitionImage(cmd, _albedoImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	
		vkCmdClearColorImage(cmd, _positionImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
		vkCmdClearColorImage(cmd, _normalImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
		vkCmdClearColorImage(cmd, _albedoImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
		
		VkUtil::TransitionImage(cmd, _positionImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkUtil::TransitionImage(cmd, _normalImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkUtil::TransitionImage(cmd, _albedoImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkUtil::TransitionImage(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		
		DrawGeometry(cmd);

		for (uint32_t i = 0; i < scene->_shadowMapCount; i++) {
			VkUtil::TransitionImage(cmd, scene->_shadowMapBuffer[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
		}

		DrawDepthPass(cmd);

		for (uint32_t i = 0; i < scene->_shadowMapCount; i++) {
			VkUtil::TransitionImage(cmd, scene->_shadowMapBuffer[i].image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
		}

		//for (uint32_t i = 0; i < scene->_shadowMapCount; i++) {
		//	VkUtil::TransitionImage(cmd, scene->_shadowMapBuffer[i].image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		//}

		VkUtil::TransitionImage(cmd, _positionImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		VkUtil::TransitionImage(cmd, _normalImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		VkUtil::TransitionImage(cmd, _albedoImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		
		DrawLightingPass(cmd);
		
		if (scene->_hasPointLights && _showDebugVolumes){
			VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			DrawDebugPass(cmd);
			VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		}
	}
	
	//transtion the draw image and the swapchain image into their correct transfer layouts
	VkUtil::TransitionImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkUtil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	
//> copyimage
	// execute a copy from the draw image into the swapchain
	VkUtil::CopyImageToImage(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex],_drawExtent ,_swapchainExtent);
//< copyimage

	// set swapchain image layout to Attachment Optimal so we can draw it
	VkUtil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//draw imgui into the swapchain image
	DrawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can draw it
	VkUtil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2KHR(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}
	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, 1,  nullptr);

	vkCmdBeginRenderingKHR(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRenderingKHR(cmd);
}

void VulkanEngine::DrawBackground(VkCommandBuffer cmd)
{
	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	// bind the background compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);	

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::DrawSkyBoxPass(VkCommandBuffer cmd){
    //begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, 1, nullptr);
	vkCmdBeginRenderingKHR(cmd, &renderInfo);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxPassPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxPassPipelineLayout, 0, 1, &_gpuSceneDataDescriptors, 0, nullptr);	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyBoxPassPipelineLayout, 1, 1, &_skyBoxPassDescriptors, 0, nullptr);

	vkCmdBindIndexBuffer(cmd, _skyBoxIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
	vkCmdEndRenderingKHR(cmd);
}

void VulkanEngine::DrawDebugPass(VkCommandBuffer cmd){
	//begin a render pass  connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_NONE);

	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, 1, &depthAttachment);
	vkCmdBeginRenderingKHR(cmd, &renderInfo);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _debugPassPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _debugPassPipelineLayout, 0, 1, &_gpuSceneDataDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _debugPassPipelineLayout, 1, 1, &_debugPassDescriptors, 0, nullptr);

	
	vkCmdBindIndexBuffer(cmd, _sphereIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexedIndirect(cmd, scene->_debugDrawCmdBuffer.buffer, 0, scene->_debugDrawCount, sizeof(VkDrawIndexedIndirectCommand));

	vkCmdEndRenderingKHR(cmd);

}

void VulkanEngine::DrawGeometry(VkCommandBuffer cmd)
{

    //begin a render pass  connected to our draw image
	// VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	// TODO create attachments for position, albedo, and normal (does this need to be done every frame?)
	VkRenderingAttachmentInfo positionAttachment = vkinit::attachment_info(_positionImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo normalAttachment = vkinit::attachment_info(_normalImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo albedoAttachment = vkinit::attachment_info(_albedoImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	std::vector<VkRenderingAttachmentInfo> colorAttachmentVec { positionAttachment, normalAttachment, albedoAttachment };

	VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, colorAttachmentVec.data(), colorAttachmentVec.size(), &depthAttachment);
	vkCmdBeginRenderingKHR(cmd, &renderInfo);

	//set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _drawExtent.width;
	viewport.height = _drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _geometryPassPipeline);

	// bind buffers
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _geometryPassPipelineLayout, 0, 1, &_gpuSceneDataDescriptors, 0, nullptr);	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _geometryPassPipelineLayout, 1, 1, &_vertexDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _geometryPassPipelineLayout, 2, 1, &_geometryPassDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _geometryPassPipelineLayout, 3, 1, &_texturesDescriptors, 0, nullptr);

	vkCmdBindIndexBuffer(cmd, scene->_modelBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexedIndirect(cmd, scene->_modelBuffers.drawCmdBuffer.buffer, 0, scene->_modelData->drawCmdBufferVec.size(), sizeof(VkDrawIndexedIndirectCommand));
	vkCmdEndRenderingKHR(cmd);
}

void VulkanEngine::DrawDepthPass(VkCommandBuffer cmd){
	for (int shadowMapIdx = 0; shadowMapIdx < scene->_shadowMapCount; shadowMapIdx++){
	//begin a render pass  connected to our draw image
		VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(scene->_shadowMapBuffer[shadowMapIdx].imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

		VkRenderingInfo renderInfo = vkinit::rendering_info(scene->_shadowMapExtent, nullptr, 0, &depthAttachment);
		renderInfo.layerCount = scene->_shadowMapLayers;
		renderInfo.viewMask = 0b111111;
		vkCmdBeginRenderingKHR(cmd, &renderInfo);

		//set dynamic viewport and scissor
		VkViewport viewport = {};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = scene->_shadowMapExtent.width;
		viewport.height = scene->_shadowMapExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = viewport.width;
		scissor.extent.height = viewport.height;

		vkCmdSetScissor(cmd, 0, 1, &scissor);

		uint32_t shadowMapPushConstants[4] = {shadowMapIdx, shadowMapIdx, shadowMapIdx, shadowMapIdx};
	
		vkCmdPushConstants(cmd, _depthPassPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t) * 4, shadowMapPushConstants);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipeline);

		// bind buffers
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipelineLayout, 0, 1, &_gpuSceneDataDescriptors, 0, nullptr);	
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipelineLayout, 1, 1, &_vertexDescriptors, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipelineLayout, 2, 1, &_geometryPassDescriptors, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipelineLayout, 3, 1, &_depthPassDescriptors, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPassPipelineLayout, 4, 1, &_lightingDataDescriptors, 0, nullptr);

		vkCmdBindIndexBuffer(cmd, scene->_modelBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexedIndirect(cmd, scene->_modelBuffers.drawCmdBuffer.buffer, 0, scene->_modelData->drawCmdBufferVec.size(), sizeof(VkDrawIndexedIndirectCommand));
		vkCmdEndRenderingKHR(cmd);	
	}
}

void VulkanEngine::DrawLightingPass(VkCommandBuffer cmd)
{
	// bind the background compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipeline);	

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 0, 1, &_gpuSceneDataDescriptors, 0, nullptr);	
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 1, 1, &_deferredPassDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 2, 1, &_drawImageDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 3, 1, &_lightingDataDescriptors, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 4, 1, &_shadowMapDescriptors, 0, nullptr);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _lightingPassPipelineLayout, 5, 1, &_depthPassDescriptors, 0, nullptr);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0), 1);
}



void VulkanEngine::HandleKeyboardInput(const SDL_KeyboardEvent& key)
{

	bool cameraUpdated = false;

	switch (key.keysym.sym)
	{
		case SDLK_TAB:
			if (key.type == SDL_KEYDOWN)
				SDL_SetRelativeMouseMode(SDL_GetRelativeMouseMode() == SDL_TRUE ? SDL_FALSE : SDL_TRUE);
			break;
		case SDLK_ESCAPE:
			bQuit = true;
			break;
		default:
		if (SDL_GetRelativeMouseMode() == SDL_TRUE)
		{
			switch (key.keysym.sym)
			{
				case SDLK_w:
				buttonState[InputAction::FORWARD] = key.type == SDL_KEYDOWN; 
				break;
			case SDLK_a:
				buttonState[InputAction::LEFT] = key.type == SDL_KEYDOWN; 
				break;
			case SDLK_s:
				buttonState[InputAction::BACKWARD] = key.type == SDL_KEYDOWN; 
				break;
			case SDLK_d:
				buttonState[InputAction::RIGHT] = key.type == SDL_KEYDOWN; 
				break;
			case SDLK_SPACE:
				buttonState[InputAction::UP] = key.type == SDL_KEYDOWN; 
				break;
			case SDLK_LSHIFT:
				buttonState[InputAction::DOWN] = key.type == SDL_KEYDOWN; 
				break;
			}	
		}
	}
	
	if (buttonState[InputAction::FORWARD])  { _camera.velocity.z = 1.0f; }
	else if (buttonState[InputAction::BACKWARD]) { _camera.velocity.z = -1.0f; }
	else { _camera.velocity.z = 0.0f; }

	if (buttonState[InputAction::LEFT]) { _camera.velocity.x = -1.0f; }
	else if (buttonState[InputAction::RIGHT]) { _camera.velocity.x = 1.0f; }
	else { _camera.velocity.x = 0.0f; }

	if (buttonState[InputAction::UP]) { _camera.velocity.y = 1.0f; }
	else if (buttonState[InputAction::DOWN]) { _camera.velocity.y = -1.0f; }
	else { _camera.velocity.y = 0.0f; }
}

// TODO: Eventually, we should abstract the input controller into a separate class
void VulkanEngine::ProcessInput(SDL_Event* e)
{
	switch (e->type){
		case SDL_MOUSEMOTION:
		if (SDL_GetRelativeMouseMode() == SDL_TRUE)
			_camera.processMouseMovement(e->motion.xrel, -e->motion.yrel);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			HandleKeyboardInput(e->key);
			break;
	}
}

void VulkanEngine::UpdateScene()
{
    // mainCamera.update();
	_camera.updatePosition(_deltaTime);
    glm::mat4 view = _camera.getViewMatrix();
	glm::mat4 invView = glm::inverse(view);

    // camera projection
    glm::mat4 projection = _camera.getProjMatrix();

	projection[1][1] *= -1;
	glm::mat4 invProj = glm::inverse(projection);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
	sceneUniformData.inverseViewMatrix = invView;
	sceneUniformData.inverseProjMatrix = invProj;
    sceneUniformData.viewProjMatrix = projection * view;
	sceneUniformData.eyePosition = glm::vec4(_camera.position, 1.0f);
}

void VulkanEngine::Run()
{
    SDL_Event e;
	
	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
			if (e.type == SDL_WINDOWEVENT) {

				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					freeze_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					freeze_rendering = false;
				}
			}
			
            ProcessInput(&e);
            ImGui_ImplSDL2_ProcessEvent(&e);
		}

        UpdateScene();

		switch (sceneLoadFlag)
		{
			case SCENE_LOAD_FLAG_CLEAR:
				scene_clear_requested = true;
				sceneLoadFlag = SCENE_LOAD_FLAG_NONE;
				break;
			case SCENE_LOAD_FLAG_FREEZE_RENDERING:
				freeze_rendering = true;
				sceneLoadFlag = SCENE_LOAD_FLAG_NONE;
				break;
			case SCENE_LOAD_FLAG_RESIZE:
				resize_requested = true;
				sceneLoadFlag = SCENE_LOAD_FLAG_NONE;
				break;
			case SCENE_LOAD_FLAG_RELOAD:
				UnloadScene();
				LoadScene(engineUIState.loadedPath);
				sceneLoadFlag = SCENE_LOAD_FLAG_NONE;
				break;
			default:
				break;
		}
		if (scene_clear_requested){
			UnloadScene();
			scene_clear_requested = false;
		}
		if (freeze_rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		if (resize_requested) {
			RecreateSwapchain();
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGui::NewFrame();

		if (ImGui::Begin("Options")) {
			VulkanEngineUI::RenderVulkanEngineUI(&engineUIState, this);
			VulkanEngineUI::RenderGlobalParamUI(&engineUIState, this);
		}

		ImGui::End();
		ImGui::Render();
		
		Draw();
        auto end = std::chrono::high_resolution_clock::now();

        _deltaTime = std::chrono::duration<float>(end - _lastTimePoint).count();
        
        _lastTimePoint = end;
        _frameTimer += _deltaTime;
	}
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2KHR(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	// allocate buffer
	VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, uint32_t arrayLayers)
{
	AllocatedImage newImage;
	newImage.imageFormat = format;
	newImage.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	img_info.arrayLayers = arrayLayers;
	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	if (arrayLayers > 1){
		view_info.subresourceRange.layerCount = img_info.arrayLayers;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

	return newImage;
}

AllocatedImage VulkanEngine::CreateImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer uploadbuffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(uploadbuffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkUtil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		// copy the buffer into the image
		vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copyRegion);

		VkUtil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

	DestroyBuffer(uploadbuffer);

	return new_image;
}

AllocatedImage VulkanEngine::CreateCubeMap(void** data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped){
	size_t data_size = size.depth * size.width * size.height * 4 * 6;
	size_t face_size = data_size / 6;

	AllocatedBuffer uploadbuffer = CreateBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	void* imageBufferPtr;
	vmaMapMemory(_allocator, uploadbuffer.allocation, &imageBufferPtr);
	for (int i = 0; i < 6; i++){
		memcpy((char*)imageBufferPtr + face_size * i, data[i], face_size);
	}
	vmaUnmapMemory(_allocator,  uploadbuffer.allocation);

	usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	AllocatedImage new_image;
	new_image.imageFormat = format;
	new_image.imageExtent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	img_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	img_info.arrayLayers = 6;

	if (mipmapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// always allocate images on dedicated GPU memory
	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &new_image.image, &new_image.allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct
	// aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	// build a image-view for the image
	VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, new_image.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;
	view_info.subresourceRange.layerCount = 6;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &new_image.imageView));

	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkUtil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		for (int i = 0; i < 6; i++){
			VkBufferImageCopy copyRegion = {};
			copyRegion.bufferOffset = face_size * i;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
	
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = i;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageExtent = size;
	
			// copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
				&copyRegion);
		}
		VkUtil::TransitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

	DestroyBuffer(uploadbuffer);
	return new_image;
}

void VulkanEngine::DestroyImage(const AllocatedImage& img)
{
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}
