#pragma once

#include <vk_types.h>

class PipelineBuilder {
//> pipeline
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    std::vector<VkFormat> _colorAttachmentFormatVec;
    std::vector<VkPipelineColorBlendAttachmentState> _colorBlendAttachmentVec;
	PipelineBuilder(){ Clear(); }

    void Clear();

    VkPipeline BuildPipeline(VkDevice device);
//< pipeline
    void SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void SetInputTopology(VkPrimitiveTopology topology);
    void SetPolygonMode(VkPolygonMode mode);
    void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void SetMultiSamplingNone();
    static VkPipelineColorBlendAttachmentState DisableBlending();
    static VkPipelineColorBlendAttachmentState EnableBlendingAdditive();
    static VkPipelineColorBlendAttachmentState EnableBlendingAlphablend();

    void AddColorAttachment(VkFormat format, VkPipelineColorBlendAttachmentState blendState);
	void SetDepthFormat(VkFormat format);
	void DisableDepthTest();
    void EnableDepthTest(bool depthWriteEnable,VkCompareOp op);
};

namespace VkUtil {
    bool LoadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}
