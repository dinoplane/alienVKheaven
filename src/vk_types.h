﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once


#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR_;
#define vkQueueSubmit2KHR vkQueueSubmit2KHR_

extern PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_;
#define vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_

extern PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_;
#define vkCmdEndRenderingKHR vkCmdEndRenderingKHR_

extern PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR_;
#define vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR_

extern PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR_;
#define vkCmdBlitImage2KHR vkCmdBlitImage2KHR_

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)


struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};


struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};


struct Vertex {

	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct Transform 
{
    glm::mat4 translation;
    glm::mat4 rotation;
    glm::mat4 scale;

    glm::mat4 GetModelMatrix() const
    {
        return translation * rotation * scale;
    }

    void SetPosition(const glm::vec3& pos)
    {
        translation = glm::translate(glm::mat4(1.0f), pos);
    }
    
    void SetRotation(const glm::vec3& rot)
    {
        rotation = glm::rotate(glm::mat4(1.0f), glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    void SetScale(const glm::vec3& scaleVec)
    {
        scale = glm::scale(glm::mat4(1.0f), scaleVec);
    }
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
};

// holds the resources needed for a model
struct GPUModelBuffers {
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer nodeTransformBuffer;
    AllocatedBuffer primitiveBuffer;
    AllocatedBuffer nodePrimitivePairBuffer;
    AllocatedBuffer materialBuffer;
    AllocatedBuffer drawCmdBuffer;
    AllocatedBuffer instanceTransformBuffer;
};




// push constants for our mesh object draws
struct GPUDrawPushConstants {
    glm::mat4 viewProjMatrix;
    glm::vec4 lightDirection;
    // glm::vec3 viewPos;
};

struct DrawContext;
// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};


struct alignas(16) GPUSphere
{                               // base alignment | aligned offset
    float centerAndRadius[4];   // 4 | 0
                                // 4 | 4
                                // 4 | 8
                                // 4 | 12
}; // 64 bytes???


struct alignas(16) GPUPlane
{                               // base alignment | aligned offset
    float normalAndDistance[4]; // 4 | 0
                                // 4 | 4
                                // 4 | 8
                                // 4 | 12
}; // 64 bytes???

struct alignas(16) GPUFrustum
{
    GPUPlane topFace;
    GPUPlane bottomFace;

    GPUPlane rightFace;
    GPUPlane leftFace;

    GPUPlane farFace;
    GPUPlane nearFace;
};

struct Plane
{
    // unit vector
    glm::vec3 normal = { 0.f, 1.f, 0.f };

    // distance from origin to the nearest point in the plane
    float     distance = 0.f;

	Plane() = default;

	Plane(const glm::vec3& p1, const glm::vec3& norm)
		: normal(glm::normalize(norm)),
		distance(glm::dot(normal, p1))
        // the normal is parallel to the vector from the nearest point to the origin
        // u dot v is measures the component of u in the v direction (where the projection equation comes from)
	{}

	float GetSignedDistanceToPlane(const glm::vec3& point) const
	{
		return glm::dot(normal, point) - distance;
	}

    GPUPlane ToGPUPlane()
    {
        return { {normal.x, normal.y, normal.z, distance} };
    }
};

struct Frustum
{
    Plane topFace;
    Plane bottomFace;

    Plane rightFace;
    Plane leftFace;

    Plane farFace;
    Plane nearFace;

    GPUFrustum ToGPUFrustum()
    {
        return { topFace.ToGPUPlane(), bottomFace.ToGPUPlane(),
            rightFace.ToGPUPlane(), leftFace.ToGPUPlane(),
            farFace.ToGPUPlane(), nearFace.ToGPUPlane() };
    }
};

