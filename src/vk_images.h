
#pragma once 
#include <vector>

namespace VkUtil {
	struct BarrierEmitter {
		std::vector<VkImageMemoryBarrier2> imageBarriers;
		// Transitions image layouts from oldLayout to newLayout using the given command buffer

		void EmitBarrier(VkCommandBuffer cmd);
		void EmitBarrierAndFlush(VkCommandBuffer cmd);

		void TransitionImage(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

		void Clear();
	};
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	// Copies the contents of one buffer to another using the given command buffer
	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);	
};