#include "vk_engine.h"

bool isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
           deviceFeatures.geometryShader;
}

PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR_ = nullptr;
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_ = nullptr;
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_ = nullptr;
PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR_ = nullptr;
PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR_ = nullptr;

void VulkanEngine::InitVulkan()
{
	vkb::InstanceBuilder builder;
	std::cout << "Initializing Vulkan 1" << std::endl;
	//make the vulkan instance, with basic debug features
	builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 2, 0);

	// // enable the VK_KHR_synchronization2 and VK_KHR_dynamic_rendering extensions if available
	auto system_info_ret = vkb::SystemInfo::get_system_info();
	if (!system_info_ret) {
		std::cerr << system_info_ret.error().message().c_str() << "\n";
		assert(false);
	}
	auto system_info = system_info_ret.value();
	// if (system_info.is_extension_available("VK_KHR_synchronization2_EXTENSION_NAME")) {
	// 	builder.enable_extension("VK_KHR_synchronization2_EXTENSION_NAME");
	// } else {
	// 	fmt::print("VK_KHR_synchronization2_EXTENSION_NAME not available\n");
	// 	assert(false);
	// }

	// if (system_info.is_extension_available("VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME")) {
	// 	builder.enable_extension("VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME");
	// } else {
	// 	fmt::print("VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME not available\n");
	// 	assert(false);
	// }

	auto inst_ret = builder.build();

	if (!inst_ret.has_value()) {
		fmt::print("Couldnt create Vulkan instance.");
		assert(false);
	}

	vkb::Instance vkb_inst = inst_ret.value();

	//grab the instance 
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;
	std::cout << "Initializing Vulkan 2" << std::endl;

	SDL_bool err = SDL_Vulkan_CreateSurface(_window, _instance, &_surface);
	if (err != SDL_TRUE) {
		fmt::print("Could not create vulkan surface: {}\n", SDL_GetError());
		assert(false);
	}

	// VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	// features.dynamicRendering = true;
	// features.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};	
	//features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;


	VkPhysicalDeviceVulkan11Features features11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};	
	features11.shaderDrawParameters = true;
	

	VkPhysicalDeviceFeatures fineGrainedFeatures{};
	fineGrainedFeatures.multiDrawIndirect = true;
	//fineGrainedFeatures.features.drawIndirectFirstInstance = VK_TRUE; 
	//deviceBuilder.add_pNext(&fineGrainedFeatures);

	std::cout << "Initializing Vulkan 3" << std::endl;

	//use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the SDL surface and supports vulkan 1.2
	vkb::PhysicalDeviceSelector selector  ( vkb_inst );
	std::cout << "Initializing Vulkan 3.1" << std::endl;

	fmt::print("{}.{}.{}\n",
		VK_API_VERSION_MAJOR(4210992),
		VK_API_VERSION_MINOR(4210992),
		VK_API_VERSION_PATCH(4210992)
	);
	
	auto physical_device_selector_return = selector
		.set_minimum_version(1, 2)
		// .set_required_features_13(features)
		 .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
		 .add_required_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)
		 .add_required_extension(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME)
		.set_required_features_12(features12)
		.set_required_features_11(features11)
		.set_required_features(fineGrainedFeatures)
		.set_surface(_surface)
		.select();


	if (!physical_device_selector_return) {
		fmt::print("Couldnt set select physical device.\n");
		assert(false);
	}
	//create the final vulkan device
	vkb::PhysicalDevice physicalDevice = physical_device_selector_return.value();

	std::cout << "Initializing Vulkan 4" << std::endl;
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };




	VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
	dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
	deviceBuilder.add_pNext(&dynamicRenderingFeatures);

	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};
	synchronization2Features.synchronization2 = VK_TRUE;
	deviceBuilder.add_pNext(&synchronization2Features);


	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;


	vkQueueSubmit2KHR_ = (PFN_vkQueueSubmit2KHR) vkGetDeviceProcAddr(_device, "vkQueueSubmit2KHR");
	vkCmdPipelineBarrier2KHR_ = (PFN_vkCmdPipelineBarrier2KHR) vkGetDeviceProcAddr(_device, "vkCmdPipelineBarrier2KHR");
	vkCmdBeginRenderingKHR_ = (PFN_vkCmdBeginRenderingKHR) vkGetDeviceProcAddr(_device, "vkCmdBeginRenderingKHR");
	vkCmdEndRenderingKHR_ = (PFN_vkCmdEndRenderingKHR) vkGetDeviceProcAddr(_device, "vkCmdEndRenderingKHR");
	vkCmdBlitImage2KHR_ = (PFN_vkCmdBlitImage2KHR) vkGetDeviceProcAddr(_device, "vkCmdBlitImage2KHR");

	std::cout << "Initializing Vulkan 5" << std::endl;
	// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	std::cout << "Initializing Vulkan 6" << std::endl;

//> vma_init
	//initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	// allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_mainDeletionQueue.push_function([&]() {
		vmaDestroyAllocator(_allocator);
	});
//< vma_init
std::cout << "Initializing Vulkan 7" << std::endl;
}

void VulkanEngine::InitSwapchain()
{
    CreateSwapchain(_windowExtent.width, _windowExtent.height);

	//depth image size will match the window
	VkExtent3D drawImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	//hardcoding the depth format to 32 bit float
	_drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	_drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

//> depthimg
	_depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	_depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

	//build a image-view for the draw image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));
//< depthimg
	//add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _drawImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

		vkDestroyImageView(_device, _depthImage.imageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
	});
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height){
    vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

	_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchainExtent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain(){
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	// destroy swapchain resources
	for (int i = 0; i < _swapchainImageViews.size(); i++) {

		vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::RecreateSwapchain(){
	vkDeviceWaitIdle(_device);

	DestroySwapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_windowExtent.width = w;
	_windowExtent.height = h;

	CreateSwapchain(_windowExtent.width, _windowExtent.height);

	resize_requested = false;
}

void VulkanEngine::InitCommands(){
    //create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
	}

	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

	// allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

	_mainDeletionQueue.push_function([=]() { 
	vkDestroyCommandPool(_device, _immCommandPool, nullptr);
	});
}

void VulkanEngine::InitSyncStructures(){
    	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
	}

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
	_mainDeletionQueue.push_function([=]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void VulkanEngine::InitDescriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocator::PoolSizeRatio> globalSizes = 
    {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }
    };

	globalDescriptorAllocator.init(_device, 10, globalSizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _postProcessPassDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    _postProcessPassDescriptors = globalDescriptorAllocator.allocate(_device, _postProcessPassDescriptorLayout);

	{
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // node transform buffer
		
        _vertexDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT);
    }
	_vertexDescriptors = globalDescriptorAllocator.allocate(_device, _vertexDescriptorLayout);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // node transform buffer
		builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // primitive buffer
		builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // node primitive index buffer
		
        _geometryPassDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT);
    }
	_geometryPassDescriptors = globalDescriptorAllocator.allocate(_device, _geometryPassDescriptorLayout);


	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = _drawImage.imageView;
	
	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = nullptr;
	
	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = _drawImageDescriptors;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

	_mainDeletionQueue.push_function([&](){
		globalDescriptorAllocator.destroy_pools(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _postProcessPassDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _geometryPassDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _vertexDescriptorLayout, nullptr);
	});



	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes = { 
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frameDescriptors = DescriptorAllocator{};
		_frames[i]._frameDescriptors.init(_device, 1000, frame_sizes);
	
		_mainDeletionQueue.push_function([&, i]() {
			_frames[i]._frameDescriptors.destroy_pools(_device);
		});
	}


	DescriptorLayoutBuilder builder;
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	_gpuSceneDataDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	builder.clear();
	builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	_singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorLayout, nullptr);
	});


}

void VulkanEngine::InitBackgroundPipelines(){
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));
		
	VkShaderModule gradientShader;
	if (!vkutil::load_shader_module("../shaders/gradient_color.comp.spv", _device, &gradientShader)) {
		std::cout << "Error when building the compute shader" << std::endl;
	}

	VkShaderModule skyShader;
	if (!vkutil::load_shader_module("../shaders/sky.comp.spv", _device, &skyShader)) {
		std::cout << "Error when building the compute shader" << std::endl;
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = _gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = _gradientPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};

	//default colors
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	//change the shader module only to create the sky shader
	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = _gradientPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	//default sky parameters
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	//add the 2 background effects into the array
	backgroundEffects.push_back(gradient);
	backgroundEffects.push_back(sky);

	//destroy structures properly
	vkDestroyShaderModule(_device, gradientShader, nullptr);
	vkDestroyShaderModule(_device, skyShader, nullptr);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
		vkDestroyPipeline(_device, sky.pipeline, nullptr);
		vkDestroyPipeline(_device, gradient.pipeline, nullptr);
	});
}

void VulkanEngine::InitGraphicsPipelines()
{
	VkShaderModule triangleFragShader;
	if (!vkutil::load_shader_module("../shaders/tex_image.frag.spv", _device, &triangleFragShader)) {
		fmt::print("Error when building the fragment shader \n");
	}
	else {
		fmt::print("Triangle fragment shader succesfully loaded \n");
	}

	VkShaderModule triangleVertexShader;
	if (!vkutil::load_shader_module("../shaders/indirect_mesh.vert.spv", _device, &triangleVertexShader)) {
		fmt::print("Error when building the vertex shader \n");
	}
	else {
		fmt::print("Triangle vertex shader succesfully loaded \n");
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	std::vector<VkDescriptorSetLayout> gltfLayoutVec;
	gltfLayoutVec.push_back(_vertexDescriptorLayout);
	gltfLayoutVec.push_back(_geometryPassDescriptorLayout);


	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	pipeline_layout_info.pPushConstantRanges = &bufferRange;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = gltfLayoutVec.data();
	pipeline_layout_info.setLayoutCount = gltfLayoutVec.size();
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));


	//exactly same as above but with the depth testing set
	PipelineBuilder pipelineBuilder;

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _meshPipelineLayout;
	//connecting the vertex and pixel shaders to the pipeline
	pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
	//it will draw triangles
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipelineBuilder.set_multisampling_none();
	//no blending
	pipelineBuilder.disable_blending();
	// pipelineBuilder.enable_blending_additive();

	//pipelineBuilder.disable_depthtest();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
	pipelineBuilder.set_depth_format(_depthImage.imageFormat);

	//finally build the pipeline
	_meshPipeline = pipelineBuilder.build_pipeline(_device);

	//clean structures
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

void VulkanEngine::InitPipelines()
{
    InitBackgroundPipelines();
    InitGraphicsPipelines();
}

void VulkanEngine::InitImgui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;


	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}

void VulkanEngine::InitDefaultData()
{
//< init_data
	// testMeshes = Loader::loadGltfMeshes(this,"..\\assets\\basicmesh.glb").value();
	std::vector<std::string> modelPaths;
	modelPaths.push_back("..\\assets\\fumo\\scene.gltf");
	modelData = Loader::LoadGltfModel(modelPaths).value();
	Loader::PrintModelData(*modelData);
	modelBuffers = Loader::LoadGeometryFromGLTF(*modelData, this);

	{
		DescriptorWriter writer;
		writer.write_buffer(0, modelBuffers.vertexBuffer.buffer, 
				modelData->vertices.size() * sizeof(Vertex), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		writer.update_set(_device, _vertexDescriptors);
	}

	{
		DescriptorWriter writer;
		writer.write_buffer(0, modelBuffers.nodeTransformBuffer.buffer, 
				modelData->nodeTransforms.size() * sizeof(glm::mat4), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		writer.write_buffer(1, modelBuffers.primitiveBuffer.buffer,
				modelData->primitives.size() * sizeof(LoadedPrimitive), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		writer.write_buffer(2, modelBuffers.nodePrimitivePairBuffer.buffer,
				modelData->nodePrimitivePairs.size() * sizeof(NodePrimitivePair), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		writer.update_set(_device, _geometryPassDescriptors);
	}

	_mainDeletionQueue.push_function([&](){
		Loader::DestroyModelData(modelBuffers, this);
	});

	// //3 default textures, white, grey, black. 1 pixel each
	// uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	// _whiteImage = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT);

	// uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	// _greyImage = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT);

	// uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	// _blackImage = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT);

	// //checkerboard image
	// uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	// std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	// for (int x = 0; x < 16; x++) {
	// 	for (int y = 0; y < 16; y++) {
	// 		pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
	// 	}
	// }
	// _errorCheckerboardImage = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
	// 	VK_IMAGE_USAGE_SAMPLED_BIT);

	// VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	// sampl.magFilter = VK_FILTER_NEAREST;
	// sampl.minFilter = VK_FILTER_NEAREST;

	// vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerNearest);

	// sampl.magFilter = VK_FILTER_LINEAR;
	// sampl.minFilter = VK_FILTER_LINEAR;
	// vkCreateSampler(_device, &sampl, nullptr, &_defaultSamplerLinear);

	// _mainDeletionQueue.push_function([&](){
	// 	vkDestroySampler(_device,_defaultSamplerNearest,nullptr);
	// 	vkDestroySampler(_device,_defaultSamplerLinear,nullptr);

	// 	destroy_image(_whiteImage);
	// 	destroy_image(_greyImage);
	// 	destroy_image(_blackImage);
	// 	destroy_image(_errorCheckerboardImage);
	// });
}
