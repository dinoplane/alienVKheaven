// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once


#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"
#include <array>
#include <iostream>
#include <fstream>
#include <chrono>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include <glm/gtx/transform.hpp>

#include "vk_mem_alloc.h"

#include "camera.h"

constexpr bool bUseValidationLayers = true;



struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	DeletionQueue _deletionQueue;
	DescriptorAllocator _frameDescriptors;
};


struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};




constexpr unsigned int FRAME_OVERLAP = 2;



class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1280 , 720 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;	

	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainExtent;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;


	// Pipeline Layouts
	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 1 };

	// Graphics
	VkPipelineLayout _geometryPassPipelineLayout;
	VkPipeline _geometryPassPipeline;

	// Compute 
	VkPipelineLayout _lightingPassPipelineLayout;
	VkPipeline _lightingPassPipeline;

	// Compute
	VkPipelineLayout _postProcessPassPipelineLayout;
	VkPipeline _postProcessPassPipeline;


	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;


	// Swapchain
	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;


	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	DescriptorAllocator globalDescriptorAllocator;



	// DescriptorSets
	VkDescriptorSetLayout _vertexDescriptorLayout;
	VkDescriptorSet _vertexDescriptors;
	
	VkDescriptorSetLayout _geometryPassDescriptorLayout;
	VkDescriptorSet _geometryPassDescriptors;

	VkDescriptorSetLayout _deferredPassDescriptorLayout;
	VkDescriptorSet _deferredPassDescriptors;

	VkDescriptorSetLayout _uberShaderPassDescriptorLayout;
	VkDescriptorSet _uberShaderPassDescriptors;

	VkDescriptorSetLayout _postProcessPassDescriptorLayout;
	VkDescriptorSet _postProcessPassDescriptors;

	VkDescriptorSetLayout _drawImageDescriptorLayout;
	VkDescriptorSet _drawImageDescriptors;

	VkDescriptorSetLayout _gltfPipelineDescriptorLayout[2];


	// immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	DeletionQueue _mainDeletionQueue;
	VmaAllocator _allocator; //vma lib allocator


	//Default Data
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	GPUMeshBuffers rectangle;
	
	// std::vector<std::shared_ptr<MeshAsset>> testMeshes;
	std::shared_ptr<LoadedGLTF> modelData;
	GPUModelBuffers modelBuffers;
	
	GPUSceneData sceneData;

	// Camera Data
	Camera _camera;

	// timing data
	float _deltaTime{ 0.0f };
	float _lastFrame{ 0.0f };
	float _frameTimer{ 0.0f };
	std::chrono::time_point<std::chrono::high_resolution_clock> _lastTimePoint;


	static VulkanEngine& Get();

	bool resize_requested{false};
	bool freeze_rendering{false};


	//initializes everything in the engine
	void init();


	//shuts down the engine
	void cleanup();


	//draw loop
	void draw();


	//run main loop
	void run();



	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	//GPUModelBuffers uploadModel(const LoadedGLTF& loadedGltf);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

	private:

	void InitVulkan();
	
	void InitSwapchain();
	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void RecreateSwapchain();

	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	
	void InitPipelines();
	void InitBackgroundPipelines();
	void InitGraphicsPipelines();


	void InitImgui();
	void InitDefaultData();

	void DrawBackground(VkCommandBuffer cmd);
	void DrawGeometry(VkCommandBuffer cmd);
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

	bool bQuit {false};
	enum InputAction {
		FORWARD = 0,
		BACKWARD = 1,
		LEFT = 2,
		RIGHT = 3,
		UP = 4,
		DOWN = 5
	};
	bool buttonState[6] {false, false, false, false, false, false};

	void ProcessInput(SDL_Event* e);
	void HandleKeyboardInput(const SDL_KeyboardEvent& key);
	void UpdateScene();
};
