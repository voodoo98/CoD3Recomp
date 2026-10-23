// Preserve CoD3's scene depth before its ALWAYS/write HUD passes replace it.
// ReShade's generic detector only recognises six-vertex fullscreen clears;
// this renderer also uses three-vertex triangles and indexed draws.
#include <reshade.hpp>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdio>

using Microsoft::WRL::ComPtr;
using namespace reshade::api;
namespace {
struct SceneDepth {
    struct Stats { uint32_t count = 0; D3D11_VIEWPORT viewport{}; D3D11_RECT scissor{}; };
    ComPtr<ID3D11Texture2D> raw, cropped, neutral;
    ComPtr<ID3D11ShaderResourceView> rawView, croppedView, neutralView;
    ComPtr<ID3D11UnorderedAccessView> croppedOutput;
    ComPtr<ID3D11ComputeShader> cropShader;
    ComPtr<ID3D11Buffer> cropConstants;
    std::unordered_map<ID3D11Texture2D*, Stats> draws;
    uint32_t score = 0, width = 0, height = 0, x = 0, y = 0;
    uint64_t frames = 0;
    bool valid = false, renderingEffects = false;
} g;

void Log(const char* text) { reshade::log::message(reshade::log::level::info, text); }

bool OnDraw(command_list* list, uint32_t, uint32_t, uint32_t, uint32_t) {
    if (list->get_device()->get_api() != device_api::d3d11 || g.renderingEffects) return false;
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(list->get_native());
    if (context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return false;
    ComPtr<ID3D11DepthStencilView> dsv;
    context->OMGetRenderTargets(0, nullptr, &dsv);
    if (!dsv) return false;
    ComPtr<ID3D11Resource> resource;
    ComPtr<ID3D11Texture2D> texture;
    dsv->GetResource(&resource);
    if (FAILED(resource.As(&texture))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    // Main scene is anamorphic; square/portrait shadow targets are excluded.
    if (desc.SampleDesc.Count != 1 || desc.Format != DXGI_FORMAT_R32G8X24_TYPELESS ||
        desc.Width < 320 || desc.Width < desc.Height * 1.2f || desc.Width > desc.Height * 2.4f) return false;
    ComPtr<ID3D11DepthStencilState> state;
    UINT stencilRef = 0;
    context->OMGetDepthStencilState(&state, &stencilRef);
    if (!state) return false;
    D3D11_DEPTH_STENCIL_DESC depth{};
    state->GetDesc(&depth);
    auto& stats = g.draws[texture.Get()];
    if (depth.DepthEnable && depth.DepthFunc != D3D11_COMPARISON_ALWAYS && depth.DepthFunc != D3D11_COMPARISON_NEVER) {
        ++stats.count;
        UINT viewports = 1;
        context->RSGetViewports(&viewports, &stats.viewport);
        UINT scissors = 1;
        context->RSGetScissorRects(&scissors, &stats.scissor);
        return false;
    }
    if (!depth.DepthEnable || depth.DepthWriteMask != D3D11_DEPTH_WRITE_MASK_ALL ||
        depth.DepthFunc != D3D11_COMPARISON_ALWAYS || stats.count < 20 || stats.count <= g.score) return false;
    const auto& viewport = stats.viewport; // The scene viewport, before HUD changes it.
    if (viewport.Width <= 0 || viewport.Height <= 0) return false;
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    D3D11_TEXTURE2D_DESC saved{};
    if (g.raw) g.raw->GetDesc(&saved);
    if (!g.raw || !g.rawView || saved.Width != desc.Width || saved.Height != desc.Height) {
        g.rawView.Reset(); g.raw.Reset();
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = 0; desc.Usage = D3D11_USAGE_DEFAULT; desc.CPUAccessFlags = 0;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.raw))) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; view.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(g.raw.Get(), &view, &g.rawView))) return false;
    }
    context->CopyResource(g.raw.Get(), texture.Get());
    g.x = std::min(desc.Width - 1, UINT(std::max(0.0f, std::round(viewport.TopLeftX))));
    g.y = std::min(desc.Height - 1, UINT(std::max(0.0f, std::round(viewport.TopLeftY))));
    g.width = std::min(desc.Width - g.x, UINT(std::max(1.0f, std::round(viewport.Width))));
    g.height = std::min(desc.Height - g.y, UINT(std::max(1.0f, std::round(viewport.Height))));
    // The recompiler applies the guest viewport in its shaders, uses a host
    // viewport covering the allocation, and scissors it to the resolved image.
    // Allocation padding must not be stretched into the scene's guide texture.
    if (stats.scissor.right > stats.scissor.left && stats.scissor.bottom > stats.scissor.top) {
        const UINT left = std::max(g.x, UINT(std::max(0L, stats.scissor.left)));
        const UINT top = std::max(g.y, UINT(std::max(0L, stats.scissor.top)));
        const UINT right = std::min(g.x + g.width, UINT(stats.scissor.right));
        const UINT bottom = std::min(g.y + g.height, UINT(stats.scissor.bottom));
        if (right > left && bottom > top) {
            g.x = left; g.y = top; g.width = right - left; g.height = bottom - top;
        }
    }
    g.score = stats.count; g.valid = true;
    return false;
}
bool OnDrawPlain(command_list* list, uint32_t v, uint32_t i, uint32_t f, uint32_t b) {
    return OnDraw(list, v, i, f, b);
}
bool OnDrawIndexed(command_list* list, uint32_t v, uint32_t i, uint32_t f, int32_t, uint32_t b) {
    return OnDraw(list, v, i, f, b);
}

bool Prepare(ID3D11Device* device) {
    if (!g.neutralView) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = DXGI_FORMAT_R32_FLOAT; desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const float farPlane = 0; // CoD3 uses reversed depth.
        const D3D11_SUBRESOURCE_DATA data{ &farPlane, sizeof(float), sizeof(float) };
        if (FAILED(device->CreateTexture2D(&desc, &data, &g.neutral)) ||
            FAILED(device->CreateShaderResourceView(g.neutral.Get(), nullptr, &g.neutralView))) return false;
    }
    if (!g.cropShader) {
        const char source[] =
            "Texture2D<float> source:register(t0); RWTexture2D<float> output:register(u0);"
            "cbuffer Crop:register(b0){uint2 offset;uint2 size;}"
            "[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID){"
            "if(all(p.xy<size)) output[p.xy]=source.Load(int3(p.xy+offset,0));}";
        ComPtr<ID3DBlob> code, errors;
        ComPtr<ID3D11ComputeShader> shader;
        ComPtr<ID3D11Buffer> constants;
        if (FAILED(D3DCompile(source, sizeof(source) - 1, nullptr, nullptr, nullptr,
            "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors)) ||
            FAILED(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader))) return false;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = 16; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&desc, nullptr, &constants))) return false;
        g.cropShader = shader; g.cropConstants = constants;
    }
    return true;
}

void OnEffects(effect_runtime* runtime, command_list* list, resource_view, resource_view) {
    if (runtime->get_device()->get_api() != device_api::d3d11) return;
    g.renderingEffects = true;
    auto* context = reinterpret_cast<ID3D11DeviceContext*>(list->get_native());
    ComPtr<ID3D11Device> device;
    context->GetDevice(&device);
    if (!Prepare(device.Get())) return;
    ID3D11ShaderResourceView* result = g.neutralView.Get();
    if (g.valid && g.rawView) {
        D3D11_TEXTURE2D_DESC desc{};
        if (g.cropped) g.cropped->GetDesc(&desc);
        if (!g.cropped || !g.croppedView || !g.croppedOutput || desc.Width != g.width || desc.Height != g.height) {
            g.croppedOutput.Reset(); g.croppedView.Reset(); g.cropped.Reset();
            desc = {}; desc.Width = g.width; desc.Height = g.height;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R32_FLOAT; desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.cropped)) ||
                FAILED(device->CreateShaderResourceView(g.cropped.Get(), nullptr, &g.croppedView)) ||
                FAILED(device->CreateUnorderedAccessView(g.cropped.Get(), nullptr, &g.croppedOutput))) {
                Log("CoD3Depth: could not allocate the scene depth crop"); return;
            }
        }
        // Only compute bindings are touched; restore each one before returning.
        ComPtr<ID3D11ComputeShader> shader;
        ID3D11ClassInstance* classes[256]{}; UINT classCount = 256;
        context->CSGetShader(&shader, classes, &classCount);
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
        ComPtr<ID3D11Buffer> constants;
        context->CSGetShaderResources(0, 1, &srv);
        context->CSGetUnorderedAccessViews(0, 1, &uav);
        context->CSGetConstantBuffers(0, 1, &constants);
        const uint32_t crop[4] = { g.x, g.y, g.width, g.height };
        context->UpdateSubresource(g.cropConstants.Get(), 0, nullptr, crop, 0, 0);
        auto* raw = g.rawView.Get(); auto* output = g.croppedOutput.Get(); auto* cb = g.cropConstants.Get();
        context->CSSetShader(g.cropShader.Get(), nullptr, 0);
        context->CSSetShaderResources(0, 1, &raw);
        context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
        context->CSSetConstantBuffers(0, 1, &cb);
        context->Dispatch((g.width + 7) / 8, (g.height + 7) / 8, 1);
        raw = srv.Get(); output = uav.Get(); cb = constants.Get();
        context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
        context->CSSetShaderResources(0, 1, &raw);
        context->CSSetConstantBuffers(0, 1, &cb);
        context->CSSetShader(shader.Get(), classes, classCount);
        for (UINT i = 0; i < classCount; ++i) classes[i]->Release();
        result = g.croppedView.Get();
        if (++g.frames <= 3 || g.frames % 600 == 0) {
            char text[160];
            snprintf(text, sizeof(text), "CoD3Depth: scene frame %llu, %ux%u at %u,%u, before HUD after %u depth-tested draws", g.frames, g.width, g.height, g.x, g.y, g.score);
            Log(text);
        }
    }
    runtime->update_texture_bindings("DEPTH", resource_view{ reinterpret_cast<uint64_t>(result) }, resource_view{ reinterpret_cast<uint64_t>(result) });
}
void OnFinish(effect_runtime* runtime, command_list*, resource_view, resource_view) {
    if (runtime->get_device()->get_api() != device_api::d3d11) return;
    g.draws.clear(); g.score = 0; g.valid = false; g.renderingEffects = false;
}
void OnDestroy(device* device) {
    if (device->get_api() == device_api::d3d11) g = SceneDepth{};
}
}
extern "C" __declspec(dllexport) const char* NAME = "CoD3 Scene Depth";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Preserves and crops the recompiled CoD3 scene depth before HUD draws.";
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        if (!reshade::register_addon(module)) return FALSE;
        reshade::register_event<reshade::addon_event::draw>(OnDrawPlain);
        reshade::register_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
        reshade::register_event<reshade::addon_event::reshade_begin_effects>(OnEffects);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(OnFinish);
        reshade::register_event<reshade::addon_event::destroy_device>(OnDestroy);
    } else if (reason == DLL_PROCESS_DETACH) {
        reshade::unregister_event<reshade::addon_event::draw>(OnDrawPlain);
        reshade::unregister_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
        reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(OnEffects);
        reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(OnFinish);
        reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroy);
        reshade::unregister_addon(module);
    }
    return TRUE;
}
