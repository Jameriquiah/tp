#pragma once
#include <cstdint>

struct RendererInitParams {
    void* window; // SDL_Window*
    uint32_t width;
    uint32_t height;
    bool vsync;
};

class RendererBackend {
public:
    virtual ~RendererBackend() = default;
    virtual bool init(const RendererInitParams& params) = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0; // present
    virtual void shutdown() = 0;
};
