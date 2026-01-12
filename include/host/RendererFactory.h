#pragma once
#include "host/RendererBackend.h"

enum class RendererType { D3D11, Vulkan };

RendererBackend* CreateRenderer(RendererType type);
void DestroyRenderer(RendererBackend* backend);
