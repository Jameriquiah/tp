#include "host/RendererBackend.h"

#if defined(_WIN32)
#include <SDL.h>
#include <SDL_syswm.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class D3D11Renderer final : public RendererBackend {
public:
    bool init(const RendererInitParams& params) override {
        window_ = static_cast<SDL_Window*>(params.window);
        vsync_ = params.vsync;

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(window_, &wmInfo) != SDL_TRUE) {
            return false;
        }
        HWND hwnd = wmInfo.info.win.window;

        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            featureLevels,
            1,
            D3D11_SDK_VERSION,
            &device_,
            &featureLevel,
            &context_);
        if (FAILED(hr)) {
            return false;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        hr = device_.As(&dxgiDevice);
        if (FAILED(hr)) {
            return false;
        }

        ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr)) {
            return false;
        }

        ComPtr<IDXGIFactory2> factory;
        hr = adapter->GetParent(__uuidof(IDXGIFactory2), &factory);
        if (FAILED(hr)) {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = params.width;
        desc.Height = params.height;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SampleDesc.Count = 1;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        hr = factory->CreateSwapChainForHwnd(
            device_.Get(),
            hwnd,
            &desc,
            nullptr,
            nullptr,
            &swapChain_);
        if (FAILED(hr)) {
            return false;
        }

        return createRenderTarget(params.width, params.height);
    }

    void resize(uint32_t width, uint32_t height) override {
        if (!swapChain_) {
            return;
        }
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        rtv_.Reset();
        swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        createRenderTarget(width, height);
    }

    void beginFrame() override {
        if (!rtv_) {
            return;
        }
        const float clearColor[4] = {0.02f, 0.02f, 0.08f, 1.0f};
        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    }

    void endFrame() override {
        if (swapChain_) {
            swapChain_->Present(vsync_ ? 1 : 0, 0);
        }
    }

    void shutdown() override {
        rtv_.Reset();
        swapChain_.Reset();
        context_.Reset();
        device_.Reset();
        window_ = nullptr;
    }

private:
    bool createRenderTarget(uint32_t width, uint32_t height) {
        ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
        if (FAILED(hr)) {
            return false;
        }
        hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_);
        if (FAILED(hr)) {
            return false;
        }
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &vp);
        return true;
    }

    SDL_Window* window_ = nullptr;
    bool vsync_ = true;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swapChain_;
    ComPtr<ID3D11RenderTargetView> rtv_;
};

RendererBackend* CreateD3D11Renderer() {
    return new D3D11Renderer();
}

#else
RendererBackend* CreateD3D11Renderer() {
    return nullptr;
}
#endif
