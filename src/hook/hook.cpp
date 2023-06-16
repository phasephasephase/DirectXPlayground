#include "hook.h"

typedef HRESULT(__thiscall* present_t)(IDXGISwapChain3*, UINT, UINT);
present_t original_present;

typedef HRESULT(__thiscall* resize_buffers_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
resize_buffers_t original_resize_buffers;

// store the game's new D3D11 device2 here
ID3D11Device* device;

HRESULT present_callback(IDXGISwapChain3* swap_chain, UINT sync_interval, UINT flags) {
    static bool once = false;
    if (!once) {
        // the game will fall back to D3D11 if the D3D12 device2 is dropped
        // useful for D2D but is kind of unstable
        ID3D12Device* bad_device;
        if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&bad_device))))
        {
            dynamic_cast<ID3D12Device5*>(bad_device)->RemoveDevice();
            return original_present(swap_chain, sync_interval, flags);
        }
        
        once = true;
    }
    
    // wait until we can get a D3D11 device2
    if (FAILED(swap_chain->GetDevice(IID_PPV_ARGS(&device))))
        return original_present(swap_chain, sync_interval, flags);
    
    // the game is now using D3D11
    // initialize renderer (this function only runs once)
    init_render(swap_chain);
    
    // draw whatever you want now!
    begin_render();
    
    draw_filled_rect(vec2_t(100, 100), vec2_t(100, 100), D2D1::ColorF(D2D1::ColorF::Red));
    draw_rect(vec2_t(100, 100), vec2_t(100, 100), D2D1::ColorF(D2D1::ColorF::Black), 5.0f);

    draw_line(vec2_t(100, 100), vec2_t(200, 200), D2D1::ColorF(D2D1::ColorF::Black), 5.0f);
    
    set_font(L"Arial");
    draw_text(L"Greetings planet - Floppy", vec2_t(100, 100), D2D1::ColorF(D2D1::ColorF::White), false, 20.0f);
    
    end_render();
    
    return original_present(swap_chain, sync_interval, flags);
}

HRESULT resize_buffers_callback(IDXGISwapChain3* swap_chain, UINT buffer_count, UINT width, 
                                UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
    // reinitialize renderer
    deinit_render();
    // init_render() will be called again in present_callback()
    
    return original_resize_buffers(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}

void install_hook() {
    // the game prefers using D3D12 over D3D11, so we'll try to hook in that same order
    if (kiero::init(kiero::RenderType::D3D12) == kiero::Status::Success) {
        // Present and ResizeBuffers live at indexes 140 and 145 respectively
        kiero::bind(140, (void**)&original_present, present_callback);
        kiero::bind(145, (void**)&original_resize_buffers, resize_buffers_callback);
        printf("Hooked D3D12.\n");
        return;
    }
    
    if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
        // indexes are 8 and 13 for D3D11 instead
        kiero::bind(8, (void**)&original_present, present_callback);
        kiero::bind(13, (void**)&original_resize_buffers, resize_buffers_callback);
        printf("Hooked D3D11.\n");
        return;
    }
    
    // something weird happened
    printf("Failed to hook.\n");
}