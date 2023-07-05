#include "render_util.h"

// silly linker error here if i name this "device"
ID3D11Device* device2 = nullptr;

IDXGISurface* back_buffer = nullptr;

ID2D1DeviceContext* context = nullptr;
ID2D1SolidColorBrush* brush = nullptr;

// things for D3D11on12
ID2D1Factory3* factory = nullptr;
ID2D1Bitmap1* bitmap;

wchar_t font[32]               = L"Segoe UI";
IDWriteFactory* write_factory  = nullptr;
IDWriteTextFormat* text_format = nullptr;

bool initialized = false;

void init_render(IDXGISwapChain3 *swap_chain) {
    if (initialized) return;
    
    swap_chain->GetDevice(IID_PPV_ARGS(&device2));
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    
    const D2D1_CREATION_PROPERTIES properties = D2D1::CreationProperties(
        D2D1_THREADING_MODE_SINGLE_THREADED,
        D2D1_DEBUG_LEVEL_NONE,
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE
    );

    D2D1CreateDeviceContext(back_buffer, properties, &context);
    back_buffer->Release();

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&write_factory);
    
    initialized = true;
}

void init_render(ID3D11On12Device* device, IDXGISwapChain3* swap_chain, ID3D12Device* d3d12_device) {
    if (initialized) return;
    
    const D2D1_FACTORY_OPTIONS options = {
        D2D1_DEBUG_LEVEL_NONE
    };
    
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &options,
                      reinterpret_cast<void**>(&factory));
    
    IDXGIDevice* dxgi_device;
    device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    
    ID2D1Device2* d2d_device;
    factory->CreateDevice(dxgi_device, &d2d_device);
    
    d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context);

    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&write_factory);
    
    // i hate D3D12 so much
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        1,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    };
    
    ID3D12DescriptorHeap* heap;
    d3d12_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap));
    
    ID3D12Resource* buffer_resource;
    swap_chain->GetBuffer(0, IID_PPV_ARGS(&buffer_resource));
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();
    d3d12_device->CreateRenderTargetView(buffer_resource, nullptr, handle);
    
    D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };
    ID3D11Resource* wrapped_buffer;
    device->CreateWrappedResource(buffer_resource, &flags, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                  D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&wrapped_buffer));
    
    IDXGISurface* surface;
    wrapped_buffer->QueryInterface(IID_PPV_ARGS(&surface));
    
    D2D1_BITMAP_PROPERTIES1 properties = {
        { DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED },
        96.0f, 96.0f,
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        nullptr
    };
    
    context->CreateBitmapFromDxgiSurface(surface, &properties, &bitmap);
    context->SetTarget(bitmap);
    
    
}

void deinit_render() {
    if (write_factory) write_factory->Release();
    if (context) context->Release();
    
    initialized = false;
}

void begin_render() {
    context->BeginDraw();
}

void end_render() {
    context->EndDraw();
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
