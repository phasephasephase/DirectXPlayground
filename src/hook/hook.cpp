#include "hook.h"

#include "../common.h"
#include "../renderer/render_util.h"

#include <kiero.h>

typedef HRESULT(__thiscall* present_t)(IDXGISwapChain3*, UINT, UINT);
present_t original_present;

typedef HRESULT(__thiscall* resize_buffers_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
resize_buffers_t original_resize_buffers;

// extra function we have to hook to get command lists for D3D11on12
typedef void(__thiscall* execute_command_lists_t)(ID3D12CommandQueue*, UINT, ID3D11CommandList* const*);
execute_command_lists_t original_execute_command_lists;

// devices for both D3D11 and D3D12
ID3D11Device* device11;
ID3D12Device* device12;

// in case we get D3D12 (stuff for D3D11on12)
ID3D12CommandQueue* command_queue;
ID3D11DeviceContext* device_context11;
ID3D11On12Device* device11on12;

IDXGISurface* back_buffer;

HRESULT present_callback(IDXGISwapChain3* swap_chain, UINT sync_interval, UINT flags) {
    static bool once = false;
    if (!once) {
        // let's see what version we got
        if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&device11)))) {
            printf("Got D3D11 device.\n");
            
            // i don't have to fiddle with D3D11on12 here!!!
            // just get the back buffer and initialize the renderer
            swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        }
        else if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&device12)))) {
            printf("Got D3D12 device.\n");

            // probably wait for command queue to be set
            if (!command_queue) {
                printf("Waiting for command queue...\n");
                return original_present(swap_chain, sync_interval, flags);
            }
            
            // some device flags for D3D11
            UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
            
            // that's about it, now we can just feed all this to D3D11on12
            // this function is a little confusing so i will comment it heavily
            HRESULT hr = D3D11On12CreateDevice(
                device12, // the D3D12 device
                device_flags, // flags for the D3D11 device
                nullptr, // array of feature levels (null is default, which is what the game uses i assume)
                0, // number of feature levels in that array
                reinterpret_cast<IUnknown**>(&command_queue), // array of command queues (we only have one)
                1, // number of command queues in that array
                0, // node mask (0 is a magic number i actually don't know what it does)
                &device11, // the D3D11 device we get back
                &device_context11, // the D3D11 device context we get back
                nullptr // the feature level we get back (we don't care)
            );

            if (FAILED(hr)) {
                printf("Failed to create D3D11on12 device, result: 0x%llX\n", hr);
                return original_present(swap_chain, sync_interval, flags);
            }
            
            // query
            hr = device11->QueryInterface(IID_PPV_ARGS(&device11on12));
            if (FAILED(hr)) {
                printf("Failed to query D3D11on12 device, result: 0x%llX\n", hr);
                return original_present(swap_chain, sync_interval, flags);
            }
            
            // print addresses of the things we got back to see if it worked
            printf("D3D11 device: %p\n", device11);
            printf("D3D11 device context: %p\n", device_context11);
            printf("D3D11on12 device: %p\n", device11on12);
        }
        else {
            printf("Failed to get device.\n");
        }
        
        once = true;
    }

    if (device11on12) {
        init_render_11on12(swap_chain, device11on12);
    }
    else {
        init_render(back_buffer);
    }

    // now we draw
    begin_render(swap_chain->GetCurrentBackBufferIndex());
    draw_filled_rect(vec2_t(10, 10), vec2_t(100, 100), D2D1::ColorF(D2D1::ColorF::Red));
    end_render(swap_chain->GetCurrentBackBufferIndex());

    if (device11on12) {
        device_context11->Flush(); // gotta flush the context too
    }
    
    return original_present(swap_chain, sync_interval, flags);
}

HRESULT resize_buffers_callback(IDXGISwapChain3* swap_chain, UINT buffer_count, UINT width, 
                                UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
    // reinitialize your renderer here
    printf("IDXGISwapChain::ResizeBuffers() was called.\n");
    
    return original_resize_buffers(swap_chain, buffer_count, width, height, new_format, swap_chain_flags);
}

void execute_command_lists_callback(ID3D12CommandQueue* this_ptr, UINT num_command_lists, 
                                    ID3D11CommandList* const* command_lists) {
    if (!command_queue) {
        command_queue = this_ptr;

        // log number of command lists
        printf("Got %d command lists.\n", num_command_lists);
    }
    
    original_execute_command_lists(this_ptr, num_command_lists, command_lists);
}

void install_hook() {
    // the game prefers using D3D12 over D3D11, so we'll try to hook in that same order
    if (kiero::init(kiero::RenderType::D3D12) == kiero::Status::Success) {
        // Present and ResizeBuffers live at indexes 140 and 145 respectively
        kiero::bind(140, (void**)&original_present, present_callback);
        kiero::bind(145, (void**)&original_resize_buffers, resize_buffers_callback);

        // we also have to hook ExecuteCommandLists for D3D11on12 (index 54)
        kiero::bind(54, (void**)&original_execute_command_lists, execute_command_lists_callback);
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