#include "render_util.h"
#include <dwrite_1.h>

ID2D1DeviceContext* context = nullptr;
ID2D1SolidColorBrush* brush = nullptr;

wchar_t font[32]               = L"Segoe UI";
IDWriteFactory* write_factory  = nullptr;
IDWriteTextFormat* text_format = nullptr;

// stuff for D3D11on12
ID3D11On12Device* device_on12 = nullptr;
std::vector<ID3D12Resource*> back_buffers;
std::vector<ID3D11Resource*> wrapped_back_buffers;
std::vector<ID2D1Bitmap1*> render_targets;
ID2D1Factory3* d2d_factory = nullptr;

bool initialized = false;

void init_render(IDXGISurface* back_buffer) {
    if (initialized) return;
    
    const D2D1_CREATION_PROPERTIES properties = D2D1::CreationProperties(
        D2D1_THREADING_MODE_SINGLE_THREADED,
        D2D1_DEBUG_LEVEL_NONE,
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE
    );

    D2D1CreateDeviceContext(back_buffer, properties, &context);

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&write_factory);
    
    initialized = true;
}

void init_render_11on12(IDXGISwapChain3* swap_chain, ID3D11On12Device* device11on12) {
    if (initialized) return;

    device_on12 = device11on12;
    
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);

    IDXGIDevice* dxgi_device;
    device11on12->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);

    ID2D1Device* d2d_device;
    d2d_factory->CreateDevice(dxgi_device, &d2d_device);
    d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context);

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&write_factory);

    // create render targets for all back buffers
    DXGI_SWAP_CHAIN_DESC desc;
    swap_chain->GetDesc(&desc);
    UINT buffer_count = desc.BufferCount;

    for (UINT i = 0; i < buffer_count; i++) {
        ID3D12Resource* back_buffer;
        swap_chain->GetBuffer(i, IID_PPV_ARGS(&back_buffer));
        back_buffers.push_back(back_buffer);
        
        D3D11_RESOURCE_FLAGS resource_flags = { };
        resource_flags.BindFlags = D3D11_BIND_RENDER_TARGET;
        
        ID3D11Resource* wrapped_back_buffer;
        device11on12->CreateWrappedResource(
            back_buffer, &resource_flags, D3D12_RESOURCE_STATE_RENDER_TARGET, 
            D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&wrapped_back_buffer)
        );
        wrapped_back_buffers.push_back(wrapped_back_buffer);

        IDXGISurface* wrapped_back_buffer_surface;
        wrapped_back_buffer->QueryInterface(IID_PPV_ARGS(&wrapped_back_buffer_surface));

        D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        
        ID2D1Bitmap1* render_target;
        context->CreateBitmapFromDxgiSurface(wrapped_back_buffer_surface, properties, &render_target);
        render_targets.push_back(render_target);
    }

    initialized = true;
}

void deinit_render() {
    if (write_factory) write_factory->Release();

    for (auto& render_target : render_targets) {
        render_target->Release();
    }

    for (auto& wrapped_back_buffer : wrapped_back_buffers) {
        wrapped_back_buffer->Release();
    }

    render_targets.clear();
    wrapped_back_buffers.clear();
    back_buffers.clear();
    
    if (context) context->Release();
    if (d2d_factory) d2d_factory->Release();
    
    initialized = false;
}

void begin_render(int index) {
    // acquire the back buffer if we're using D3D11on12
    if (!back_buffers.empty()) {
        device_on12->AcquireWrappedResources(&wrapped_back_buffers[index], 1);
    }
    
    // set the correct render target if we're using D3D11on12
    if (!render_targets.empty()) {
        context->SetTarget(render_targets[index]);
    }
    
    context->BeginDraw();
}

void end_render(int index) {
    context->EndDraw();

    if (!back_buffers.empty()) {
        device_on12->ReleaseWrappedResources(&wrapped_back_buffers[index], 1);
    }
}

void draw_rect(vec2_t pos, vec2_t size, D2D1::ColorF color, float thickness) {
    context->CreateSolidColorBrush(color, &brush);
    context->DrawRectangle(D2D1::RectF(pos.x, pos.y, pos.x + size.x, pos.y + size.y), brush, thickness);
    brush->Release();
}

void draw_filled_rect(vec2_t pos, vec2_t size, D2D1::ColorF color) {
    context->CreateSolidColorBrush(color, &brush);
    context->FillRectangle(D2D1::RectF(pos.x, pos.y, pos.x + size.x, pos.y + size.y), brush);
    brush->Release();
}

void draw_line(vec2_t start, vec2_t end, D2D1::ColorF color, float thickness) {
    context->CreateSolidColorBrush(color, &brush);
    context->DrawLine(D2D1::Point2F(start.x, start.y), D2D1::Point2F(end.x, end.y), brush, thickness);
    brush->Release();
}

void set_font(const wchar_t* font_name) {
    wcscpy_s(font, font_name);
}

void draw_text(const wchar_t* text, vec2_t pos, D2D1::ColorF color, bool shadow, float size) {
    write_factory->CreateTextFormat(font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &text_format);
    text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    
    if (shadow) {
        context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF(0, 0, 0, 0.5f)), &brush);
        context->DrawTextA(text, wcslen(text), text_format, 
            D2D1::RectF(pos.x + 2, pos.y + 2, pos.x + 10000, pos.y + 10000), brush);
        brush->Release();
    }
    
    context->CreateSolidColorBrush(color, &brush);
    context->DrawTextA(text, wcslen(text), text_format, 
        D2D1::RectF(pos.x, pos.y, pos.x + 10000, pos.y + 10000), brush);
    
    brush->Release();
    text_format->Release();
}

// private function
DWRITE_TEXT_METRICS get_metrics(const wchar_t* text, float size) {
    write_factory->CreateTextFormat(font, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &text_format);
    text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    
    IDWriteTextLayout* layout;
    write_factory->CreateTextLayout(text, wcslen(text), text_format, 10000, 10000, &layout);
    
    DWRITE_TEXT_METRICS metrics { };
    layout->GetMetrics(&metrics);
    
    layout->Release();
    text_format->Release();
    
    return metrics;
}

float text_width(const wchar_t* text, float size) {
    return get_metrics(text, size).width;
}

float text_height(const wchar_t* text, float size) {
    return get_metrics(text, size).height;
}
