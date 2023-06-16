#include "hook.h"

typedef HRESULT(__thiscall* present_t)(IDXGISwapChain3*, UINT, UINT);
present_t original_present;

typedef HRESULT(__thiscall* resize_buffers_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
resize_buffers_t original_resize_buffers;

// devices for both D3D11 and D3D12
ID3D11Device* device11;
ID3D12Device* device12;

// in case we get D3D12 (stuff for D3D11on12)
ID3D12CommandQueue* command_queue;
ID3D11DeviceContext* device_context11;

HRESULT present_callback(IDXGISwapChain3* swap_chain, UINT sync_interval, UINT flags) {
    static bool once = false;
    if (!once) {
        // let's see what version we got
        if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&device11)))) {
            printf("Got D3D11 device.\n");
            
            // i don't have to fiddle with D3D11on12 here!!!
        }
        else if (SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&device12)))) {
            printf("Got D3D12 device.\n");
            
            // make a command queue
            D3D12_COMMAND_QUEUE_DESC queue_desc = {};
            queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            
            if (FAILED(device12->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)))) {
                printf("Failed to create command queue.\n");
            }
            
            // some device flags for D3D11
            UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
            
            // that's about it, now we can just feed all this to D3D11on12
            // this function is a little confusing so i will comment it heavily
            D3D11On12CreateDevice(
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
            
            // print addresses of the things we got back to see if it worked
            printf("D3D11 device: %p\n", device11);
            printf("D3D11 device context: %p\n", device_context11);
        }
        else {
            printf("Failed to get device.\n");
        }
        
        once = true;
    }
    
    return original_present(swap_chain, sync_interval, flags);
}

HRESULT resize_buffers_callback(IDXGISwapChain3* swap_chain, UINT buffer_count, UINT width, 
                                UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags) {
    // reinitialize your renderer here
    printf("IDXGISwapChain::ResizeBuffers() was called.\n");
    
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