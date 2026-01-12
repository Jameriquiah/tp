#include "host/RendererFactory.h"
#include "host/HostAssets.h"
#include "host/HostGame.h"
#include <SDL.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

static RendererType pickRenderer(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--renderer=vulkan") == 0) {
            return RendererType::Vulkan;
        }
        if (std::strcmp(argv[i], "--renderer=d3d11") == 0) {
            return RendererType::D3D11;
        }
    }
#if defined(_WIN32)
    return RendererType::D3D11;
#else
    return RendererType::Vulkan;
#endif
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        return 1;
    }

    std::printf("Asset root: %s\n", HostGetAssetRoot());

    RendererType type = pickRenderer(argc, argv);
    uint32_t width = 1280;
    uint32_t height = 720;
    Uint32 flags = SDL_WINDOW_RESIZABLE;
    if (type == RendererType::Vulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    SDL_Window* window = SDL_CreateWindow(
        "TP Host",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        static_cast<int>(width),
        static_cast<int>(height),
        flags);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    RendererBackend* renderer = CreateRenderer(type);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RendererInitParams initParams = {};
    initParams.window = window;
    initParams.width = width;
    initParams.height = height;
    initParams.vsync = true;

    if (!renderer->init(initParams)) {
        DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    HostGameBootstrap();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                renderer->resize(static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2));
            }
        }

        renderer->beginFrame();
        renderer->endFrame();
    }

    renderer->shutdown();
    DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
