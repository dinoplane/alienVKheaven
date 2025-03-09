
#pragma once 

namespace VkUtil {
	// Transitions image layouts from oldLayout to newLayout using the given command buffer
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	// Copies the contents of one buffer to another using the given command buffer
	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);	
};