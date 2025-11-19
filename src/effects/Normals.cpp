#include "Normals.h"

#include "../../Shared/Bridge/Bridge.h"
#include "../core/TextureManager.h"

void NormalsEffect::RegisterConstants() {
	TheShaderManager->RegisterConstant("TESR_NormalsData", &Constants.Data);
}
	
void NormalsEffect::RegisterTextures() {
        TheTextureManager->InitTexture("TESR_NormalsBuffer", &Textures.NormalsTexture, &Textures.NormalsSurface, TheRenderManager->width, TheRenderManager->height, D3DFMT_A16B16G16R16F);
        TextureManager::RegisterBridgeRenderTarget("TESR_NormalsBuffer",
                &Textures.NormalsTexture,
                &Textures.NormalsSurface,
                TR_BRIDGE_RT_USAGE_COLOR_BIT | TR_BRIDGE_RT_USAGE_SAMPLED_BIT);
        TextureManager::PublishBridgeState();
}