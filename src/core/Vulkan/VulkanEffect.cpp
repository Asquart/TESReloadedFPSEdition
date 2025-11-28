#include "VulkanEffect.h"

IVulkanEffect::~IVulkanEffect()
{
    DestroyResources();
}

void IVulkanEffect::Initialize()
{
    CreateResources();
}

void IVulkanEffect::OnGameBuffersUpdated()
{
    CreateDescriptorSets();
}

std::vector<uint32_t> IVulkanEffect::LoadSpirv(const char* path)
{
    Logger::Log("Trying to get shader file : %s", path);
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

void IVulkanEffect::CreateResources()
{
    CreateShaderModule();
    CreateInteropTextures();
    CreatePipeline();
    CreateDescriptorSets();
}

void IVulkanEffect::DestroyResources()
{
}
