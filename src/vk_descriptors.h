#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void AddBinding(uint32_t binding, VkDescriptorType type);
    void Clear();
    VkDescriptorSetLayout BuildLayout(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};


struct DescriptorAllocator {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void Init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	void ClearPools(VkDevice device);
	void DestroyPools(VkDevice device);

    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
	VkDescriptorPool GetPool(VkDevice device);
	VkDescriptorPool CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};

struct DescriptorWriter {
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
    
	// Clears all the image infos, buffer infos, and writes
	void Clear();

	// Adds a single image info struct and its corresponding write struct
    void WriteSingleImage(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
	
	// Adds an image info struct
	void AddImageInfo(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
	
	// Creates a write struct for the image infos added
	void CommitImageWrite(int binding, VkDescriptorType type);
    
	// Adds a single buffer info struct and its corresponding write struct
	void WriteBuffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 

	// Updates the descriptor set with given writes
    void ApplyDescriptorSetUpdates(VkDevice device, VkDescriptorSet set);
};
