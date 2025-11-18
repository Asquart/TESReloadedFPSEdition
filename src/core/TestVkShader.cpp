#include "TestVkShader.h"

void TestVkShader::Initialize()
{
    TheVulkanTestShader = new TestVkShader();
}

void TestVkShader::InitCompute(IDirect3DDevice9* InD3D9Device)
{
    //dxvk::Com<ID3D9VkInteropDevice> VulkanDevice = nullptr;
    //VkInstance Instance = VK_NULL_HANDLE;
    //VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    //VkDevice              Device = VK_NULL_HANDLE;
    //VkQueue Queue = VK_NULL_HANDLE;
    //uint32_t FamilyIndex = -1;
    //uint32_t QueueIndex = -1;

    InD3D9Device->QueryInterface(__uuidof(ID3D9VkInteropDevice), (void**)&ComputeContext.VulkanDevice);

    ComputeContext.VulkanDevice->GetVulkanHandles(&ComputeContext.Instance, &ComputeContext.PhysicalDevice, &ComputeContext.Device);
    ComputeContext.VulkanDevice->GetSubmissionQueue(&ComputeContext.Queue, &ComputeContext.QueueIndex, &ComputeContext.FamilyIndex);

    VkDispatch::InitVulkanFunctionPointers(ComputeContext.Instance, ComputeContext.Device);

    Logger::Log("TestVkShader initialized using device: %d", ComputeContext.Device);

    Logger::Log("Trying to LoadSpirv()");
    auto spirv = LoadSpirv("Data\\Shaders\\NewVegasReloaded\\Shaders\\SolidColor.comp.spv");
    if (spirv.empty()) {
        // handle error (file missing, etc.)
        Logger::Log("Could not load SPIR-V shader file");
        return;
    }

    const uint32_t* spirvData = spirv.data();
    size_t          spirvSizeBytes = spirv.size() * sizeof(uint32_t);

    // 1) Shader module (SPIR-V from your AO compute shader)
    VkShaderModuleCreateInfo smInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smInfo.codeSize = spirvSizeBytes;
    smInfo.pCode = spirvData;

    Logger::Log("Trying to call p_vkCreateShaderModule()");
    if (!p_vkCreateShaderModule)
    {
        Logger::Log("p_vkCreateShaderModule() is invalid");
    }
    else
    {
        Logger::Log("p_vkCreateShaderModule() is valid");
    }
    VK_CHECK(p_vkCreateShaderModule(ComputeContext.Device, &smInfo, nullptr, &ComputeContext.shader), "p_vkCreateShaderModule");
    Logger::Log("Successfully called p_vkCreateShaderModule()");

    // 2) Descriptor set layout: binding 0 = storage image
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dslInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &binding;
    VK_CHECK(p_vkCreateDescriptorSetLayout(ComputeContext.Device, &dslInfo, nullptr, &ComputeContext.descSetLayout), "p_vkCreateDescriptorSetLayout");

    // 3) Pipeline layout
    // Describe the push constant block:
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;          // first byte
    pcRange.size = sizeof(float) * 4;  // vec4 = 16 bytes
    VkPipelineLayoutCreateInfo plInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &ComputeContext.descSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    VK_CHECK(p_vkCreatePipelineLayout(ComputeContext.Device, &plInfo, nullptr, &ComputeContext.pipelineLayout), "p_vkCreatePipelineLayout");

    // 4) Compute pipeline
    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = ComputeContext.shader;
    stage.pName = "main"; // entry point

    VkComputePipelineCreateInfo cpInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    cpInfo.stage = stage;
    cpInfo.layout = ComputeContext.pipelineLayout;
    VK_CHECK(p_vkCreateComputePipelines(ComputeContext.Device, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &ComputeContext.pipeline), "p_vkCreateComputePipelines");

    // 5) Descriptor pool (just some storage image descriptors)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 64; // enough for multiple frames

    VkDescriptorPoolCreateInfo dpInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpInfo.maxSets = 64;
    dpInfo.poolSizeCount = 1;
    dpInfo.pPoolSizes = &poolSize;
    VK_CHECK(p_vkCreateDescriptorPool(ComputeContext.Device, &dpInfo, nullptr, &ComputeContext.descPool),
        "vkCreateDescriptorPool");

    // 6) Command pool for the queue we’ll use
    VkCommandPoolCreateInfo cpPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpPoolInfo.queueFamilyIndex = ComputeContext.FamilyIndex;
    VK_CHECK(p_vkCreateCommandPool(ComputeContext.Device, &cpPoolInfo, nullptr, &ComputeContext.cmdPool), "p_vkCreateCommandPool");
}

VkImageView TestVkShader::GetOrCreateViewForImage(VkImage image, VkFormat format, uint32_t mipLevels, uint32_t arrayLayers)
{
    auto it = VkImageViews.find(image);
    if (it != VkImageViews.end())
        return it->second;

    VkImageViewCreateInfo ivInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivInfo.image = image;
    ivInfo.viewType = (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    ivInfo.format = format;

    ivInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivInfo.subresourceRange.baseMipLevel = 0;
    ivInfo.subresourceRange.levelCount = 1; // usually AO RT is single mip
    ivInfo.subresourceRange.baseArrayLayer = 0;
    ivInfo.subresourceRange.layerCount = arrayLayers;

    VkImageView view;
    p_vkCreateImageView(ComputeContext.Device, &ivInfo, nullptr, &view);
    VkImageViews[image] = view;
    return view;
}

void TestVkShader::RunCompute(IDirect3DSurface9* InD3D9Surface)
{
    Logger::Log("Running compute with Device: %d, Queue: %d, QueueIndex: %d, FamilyIndex: %d, Instance: %d, PhysicalDevice: %d,", ComputeContext.Device, ComputeContext.Queue, ComputeContext.QueueIndex, ComputeContext.FamilyIndex, ComputeContext.Instance, ComputeContext.PhysicalDevice);

    dxvk::Com<ID3D9VkInteropTexture> VkTexture;
    InD3D9Surface->QueryInterface(__uuidof(ID3D9VkInteropTexture), (void**)&VkTexture);

    VulkanImageData NewImageData{};
    VkTexture->GetVulkanImageInfo(&NewImageData.Image, &NewImageData.ImageLayout, &NewImageData.ImageCreateInfo);

    VkImageSubresourceRange Range{};
    Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Range.baseMipLevel = 0;
    Range.levelCount = NewImageData.ImageCreateInfo.mipLevels;
    Range.baseArrayLayer = 0;
    Range.layerCount = NewImageData.ImageCreateInfo.arrayLayers;

    ComputeContext.VulkanDevice->LockSubmissionQueue();

    VkImageLayout originalLayout = NewImageData.ImageLayout;
    VkImageLayout computeLayout = VK_IMAGE_LAYOUT_GENERAL;

    ComputeContext.VulkanDevice->TransitionTextureLayout(VkTexture.ptr(), &Range,
        originalLayout,
        computeLayout);


    VkImageView view = GetOrCreateViewForImage(
        NewImageData.Image,
        NewImageData.ImageCreateInfo.format,
        NewImageData.ImageCreateInfo.mipLevels,
        NewImageData.ImageCreateInfo.arrayLayers
    );

    // 2) Allocate descriptor set
    VkDescriptorSetAllocateInfo dsAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsAlloc.descriptorPool = ComputeContext.descPool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts = &ComputeContext.descSetLayout;

    VkDescriptorSet descSet;
    VK_CHECK(p_vkAllocateDescriptorSets(ComputeContext.Device, &dsAlloc, &descSet), "p_vkAllocateDescriptorSets");

    // 3) Bind the storage image
    VkDescriptorImageInfo imgDesc{};
    imgDesc.imageView = view;
    imgDesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // we ensured this before

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet = descSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &imgDesc;

    p_vkUpdateDescriptorSets(ComputeContext.Device, 1, &write, 0, nullptr);

    // 4) Allocate command buffer
    VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAlloc.commandPool = ComputeContext.cmdPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(p_vkAllocateCommandBuffers(ComputeContext.Device, &cbAlloc, &cmd), "p_vkAllocateCommandBuffers");

    // 5) Begin command buffer
    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(p_vkBeginCommandBuffer(cmd, &beginInfo), "p_vkBeginCommandBuffer");

    // 6) Bind compute pipeline + descriptor set
    p_vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ComputeContext.pipeline);

    PushData pc{};
    pc.color[0] = 0.0f;  // R
    pc.color[1] = 1.0f;  // G
    pc.color[2] = 1.0f;  // B
    pc.color[3] = 1.0f;  // A
    p_vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        ComputeContext.pipelineLayout, 0, 1, &descSet,
        0, nullptr);

    // 7) Dispatch
    // Choose workgroup size (e.g. 8x8 or 16x16) to match your shader.
    const uint32_t wgSizeX = 16;
    const uint32_t wgSizeY = 16;

    uint32_t groupsX = (NewImageData.ImageCreateInfo.extent.width + wgSizeX - 1) / wgSizeX;
    uint32_t groupsY = (NewImageData.ImageCreateInfo.extent.height + wgSizeY - 1) / wgSizeY;
    p_vkCmdPushConstants(cmd, ComputeContext.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &pc);
    p_vkCmdDispatch(cmd, groupsX, groupsY, NewImageData.ImageCreateInfo.arrayLayers);

    // 8) End command buffer
    VK_CHECK(p_vkEndCommandBuffer(cmd), "VK_CHECK");

    // 9) Submit + sync (simple version: wait immediately)
    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VK_CHECK(p_vkCreateFence(ComputeContext.Device, &fenceInfo, nullptr, &fence), "p_vkCreateFence");

    p_vkQueueSubmit(ComputeContext.Queue, 1, &submit, fence);
    VK_CHECK(p_vkWaitForFences(ComputeContext.Device, 1, &fence, VK_TRUE, UINT64_MAX), "p_vkWaitForFences");

    p_vkDestroyFence(ComputeContext.Device, fence, nullptr);

    p_vkFreeCommandBuffers(ComputeContext.Device, ComputeContext.cmdPool, 1, &cmd);

    p_vkFreeDescriptorSets(ComputeContext.Device, ComputeContext.descPool, 1, &descSet);


    ComputeContext.VulkanDevice->TransitionTextureLayout(VkTexture.ptr(), &Range,
        computeLayout,
        originalLayout);

    ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
    // You can also recycle descriptor sets or just let them accumulate if pool is big enough.
}

void TestVkShader::ClearSurfaceVulkan(IDirect3DSurface9* surface)
{
    if (!ComputeContext.Device || !ComputeContext.Queue || !ComputeContext.VulkanDevice.ptr()) {
        Logger::Log("ClearSurfaceVulkan: missing device/queue/interp");
        return;
    }

    Logger::Log("ClearSurfaceVulkan on surface=%p", surface);

    dxvk::Com<ID3D9VkInteropTexture> vkTex;
    if (FAILED(surface->QueryInterface(__uuidof(ID3D9VkInteropTexture), (void**)&vkTex))) {
        Logger::Log("ClearSurfaceVulkan: QI ID3D9VkInteropTexture failed");
        return;
    }

    VulkanImageData img{};
    vkTex->GetVulkanImageInfo(&img.Image, &img.ImageLayout, &img.ImageCreateInfo);

    Logger::Log("ClearSurfaceVulkan: image=%p, fmt=%d, w=%u, h=%u, layers=%u, usage=0x%08X, layout=%d",
        img.Image,
        img.ImageCreateInfo.format,
        img.ImageCreateInfo.extent.width,
        img.ImageCreateInfo.extent.height,
        img.ImageCreateInfo.arrayLayers,
        img.ImageCreateInfo.usage,
        img.ImageLayout);

    if (img.Image == VK_NULL_HANDLE) {
        Logger::Log("ClearSurfaceVulkan: null VkImage");
        return;
    }

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = img.ImageCreateInfo.mipLevels ? img.ImageCreateInfo.mipLevels : 1;
    range.baseArrayLayer = 0;
    range.layerCount = img.ImageCreateInfo.arrayLayers ? img.ImageCreateInfo.arrayLayers : 1;

    VkImageLayout originalLayout = img.ImageLayout;
    VkImageLayout clearLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    ComputeContext.VulkanDevice->LockSubmissionQueue();

    // Transition to TRANSFER_DST
    ComputeContext.VulkanDevice->TransitionTextureLayout(
        vkTex.ptr(), &range,
        originalLayout,
        clearLayout);

    // Allocate a one-off command buffer
    VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAlloc.commandPool = ComputeContext.cmdPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkResult vr = p_vkAllocateCommandBuffers(ComputeContext.Device, &cbAlloc, &cmd);
    Logger::Log("ClearSurfaceVulkan: vkAllocateCommandBuffers -> %d, cmd=%p", (int)vr, cmd);
    if (vr != VK_SUCCESS || !cmd) {
        ComputeContext.VulkanDevice->TransitionTextureLayout(
            vkTex.ptr(), &range,
            clearLayout,
            originalLayout);
        ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
        return;
    }

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vr = p_vkBeginCommandBuffer(cmd, &beginInfo);
    Logger::Log("ClearSurfaceVulkan: vkBeginCommandBuffer -> %d", (int)vr);
    if (vr != VK_SUCCESS) {
        p_vkFreeCommandBuffers(ComputeContext.Device, ComputeContext.cmdPool, 1, &cmd);
        ComputeContext.VulkanDevice->TransitionTextureLayout(
            vkTex.ptr(), &range,
            clearLayout,
            originalLayout);
        ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
        return;
    }

    // Clear color (bright magenta)
    VkClearColorValue clearColor{};
    clearColor.float32[0] = 1.0f; // R
    clearColor.float32[1] = 0.0f; // G
    clearColor.float32[2] = 1.0f; // B
    clearColor.float32[3] = 1.0f; // A

    p_vkCmdClearColorImage(
        cmd,
        img.Image,
        clearLayout,
        &clearColor,
        1,
        &range);

    vr = p_vkEndCommandBuffer(cmd);
    Logger::Log("ClearSurfaceVulkan: vkEndCommandBuffer -> %d", (int)vr);
    if (vr != VK_SUCCESS) {
        p_vkFreeCommandBuffers(ComputeContext.Device, ComputeContext.cmdPool, 1, &cmd);
        ComputeContext.VulkanDevice->TransitionTextureLayout(
            vkTex.ptr(), &range,
            clearLayout,
            originalLayout);
        ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
        return;
    }

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    vr = p_vkCreateFence(ComputeContext.Device, &fenceInfo, nullptr, &fence);
    Logger::Log("ClearSurfaceVulkan: vkCreateFence -> %d, fence=%p", (int)vr, fence);
    if (vr != VK_SUCCESS || !fence) {
        p_vkFreeCommandBuffers(ComputeContext.Device, ComputeContext.cmdPool, 1, &cmd);
        ComputeContext.VulkanDevice->TransitionTextureLayout(
            vkTex.ptr(), &range,
            clearLayout,
            originalLayout);
        ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
        return;
    }

    vr = p_vkQueueSubmit(ComputeContext.Queue, 1, &submit, fence);
    Logger::Log("ClearSurfaceVulkan: vkQueueSubmit -> %d", (int)vr);

    if (vr == VK_SUCCESS) {
        vr = p_vkWaitForFences(ComputeContext.Device, 1, &fence, VK_TRUE, UINT64_MAX);
        Logger::Log("ClearSurfaceVulkan: vkWaitForFences -> %d", (int)vr);
    }

    p_vkDestroyFence(ComputeContext.Device, fence, nullptr);
    p_vkFreeCommandBuffers(ComputeContext.Device, ComputeContext.cmdPool, 1, &cmd);

    // Transition back
    ComputeContext.VulkanDevice->TransitionTextureLayout(
        vkTex.ptr(), &range,
        clearLayout,
        originalLayout);

    ComputeContext.VulkanDevice->ReleaseSubmissionQueue();
}

std::vector<uint32_t> TestVkShader::LoadSpirv(const char* path)
{
    Logger::Log("Trying to get shader file");
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        Logger::Log("Could not find valid shader file, returning {}");
        return {};
    }

    Logger::Log("Got valid shader file");
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    Logger::Log("Reading shader file");
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        return {};

    return buffer;
}
