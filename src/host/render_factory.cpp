#include "host/RendererFactory.h"

RendererBackend* CreateD3D11Renderer();
RendererBackend* CreateVulkanRenderer();

RendererBackend* CreateRenderer(RendererType type) {
    switch (type) {
    case RendererType::D3D11:
        return CreateD3D11Renderer();
    case RendererType::Vulkan:
        return CreateVulkanRenderer();
    default:
        return nullptr;
    }
}

void DestroyRenderer(RendererBackend* backend) {
    delete backend;
}
