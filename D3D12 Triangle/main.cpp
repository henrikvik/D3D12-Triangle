#include <d3d12.h>
#include <memory>
#include <dxgi1_5.h>
#include <stdexcept>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


#define SafeRelease(ptr) { if (ptr) ptr->Release(); ptr = nullptr; }

HWND create_window(HINSTANCE instance, INT cmd_show, UINT width, UINT height);
LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);


IDXGIAdapter1 * find_adapter();

ID3D12Device *              create_device(IDXGIAdapter1 * adapter);
ID3D12CommandQueue*         create_cmd_queue(ID3D12Device * device);
ID3D12CommandAllocator *    create_cmd_allocator(ID3D12Device * device);
ID3D12GraphicsCommandList * create_cmd_list(ID3D12Device * device, ID3D12CommandAllocator * cmd_allocator);
IDXGISwapChain1 *           create_swap_chain(HWND wnd_handle, ID3D12CommandQueue * cmd_queue, UINT num_swap_buffers);

void wait_for_gpu();


// Global variables
struct Globals
{
    static const UINT NUM_SWAP_BUFFERS = 1;

    ID3D12Device * device        = nullptr;

    ID3D12CommandQueue *        cmd_queue     = nullptr;
    ID3D12CommandAllocator *    cmd_allocator = nullptr;
    ID3D12GraphicsCommandList * cmd_list      = nullptr;

    ID3D12Fence * fence = nullptr;
    HANDLE fence_event = nullptr;
    UINT64 fence_value = 1;

    IDXGISwapChain1 * swap_chain = nullptr;

    ID3D12DescriptorHeap * render_targets_heap = nullptr;
    ID3D12Resource * render_targets[NUM_SWAP_BUFFERS] = { nullptr };

    ~Globals()
    {
        SafeRelease(device);

        SafeRelease(cmd_queue);
        SafeRelease(cmd_allocator);
        SafeRelease(cmd_list);

        SafeRelease(fence);
        SafeRelease(swap_chain);

        SafeRelease(render_targets_heap);


    }
} g;

INT WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, INT cmd_show)
{
try
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    HWND wnd_handle = create_window(instance, cmd_show, 512, 512);

    auto adapter = find_adapter();

    g.device        = create_device(adapter);
    g.cmd_queue     = create_cmd_queue(g.device);
    g.cmd_allocator = create_cmd_allocator(g.device);
    g.cmd_list      = create_cmd_list(g.device, g.cmd_allocator);

    g.swap_chain = create_swap_chain(wnd_handle, g.cmd_queue, g.NUM_SWAP_BUFFERS);

    SafeRelease(adapter);

    return 0;
}
catch (std::exception * e)
{
    OutputDebugString(e->what());
    return EXIT_FAILURE;
}
}

HWND create_window(HINSTANCE instance, INT cmd_show, UINT width, UINT height)
{
    const char CLASS_NAME[] = "D3D12 Triangle";

    WNDCLASS wnd_class = {};
    wnd_class.lpfnWndProc = WindowProc;
    wnd_class.hInstance = instance;
    wnd_class.lpszClassName = CLASS_NAME;

    RegisterClass(&wnd_class);

    HWND window = CreateWindowEx(
        0,
        CLASS_NAME,
        CLASS_NAME,                     // Window Title
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,   // Position
        width, height,                  // Size
        NULL,
        NULL,
        instance,
        NULL
    );

    if (window == NULL)
    {
        throw std::runtime_error("Failed to create Window");
    }

    ShowWindow(window, cmd_show);

    //MSG msg = {};
    //while (GetMessage(&msg, NULL, 0, 0))
    //{
    //    TranslateMessage(&msg);
    //    DispatchMessage(&msg);
    //}

    return window;
}

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(window, msg, wparam, lparam);
}

IDXGIAdapter1 * find_adapter()
{
    IDXGIFactory5* factory = nullptr;
    IDXGIAdapter1* adapter = nullptr;

    CreateDXGIFactory(IID_PPV_ARGS(&factory));
    for (UINT adapterIndex = 0;; adapterIndex++)
    {
        if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
        {
            break;
        }

        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
        {
            break;
        }

        SafeRelease(adapter);
    }

    SafeRelease(factory);

    return adapter;
}

ID3D12Device * create_device(IDXGIAdapter1 * adapter)
{
    ID3D12Device * device = nullptr;

    if (adapter)
    {
        D3D12CreateDevice(
            adapter, 
            D3D_FEATURE_LEVEL_12_1,
            IID_PPV_ARGS(&device)
        );
    }
    else
    {
        IDXGIFactory5* factory = nullptr;
        CreateDXGIFactory(IID_PPV_ARGS(&factory));
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));

        D3D12CreateDevice(
            adapter,
            D3D_FEATURE_LEVEL_11_0, 
            IID_PPV_ARGS(&device)
        );

        SafeRelease(factory);
    }

    return device;
}

ID3D12CommandQueue * create_cmd_queue(ID3D12Device * device)
{
    ID3D12CommandQueue * cmd_queue = nullptr;

    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc = {};
    device->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&cmd_queue));

    return cmd_queue;
}

ID3D12CommandAllocator * create_cmd_allocator(ID3D12Device * device)
{
    ID3D12CommandAllocator * cmd_allocator = nullptr;

    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmd_allocator));

    return cmd_allocator;
}

ID3D12GraphicsCommandList* create_cmd_list(ID3D12Device * device, ID3D12CommandAllocator * cmd_allocator)
{
    ID3D12GraphicsCommandList * cmd_list = nullptr;

    device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        cmd_allocator,
        nullptr,
        IID_PPV_ARGS(&cmd_list)
    );

    cmd_list->Close();

    return cmd_list;
}

IDXGISwapChain1 * create_swap_chain(HWND wnd_handle, ID3D12CommandQueue * cmd_queue, UINT num_swap_buffers)
{
    IDXGIFactory5 * factory = nullptr;
    CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));


    IDXGISwapChain1 * swap_chain = nullptr;

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc1 = {};

    swap_chain_desc1.Width = 512;
    swap_chain_desc1.Height = 512;
    swap_chain_desc1.Scaling = DXGI_SCALING_NONE;
    swap_chain_desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc1.BufferCount = num_swap_buffers;
    swap_chain_desc1.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swap_chain_desc1.SampleDesc.Count = 1;
    swap_chain_desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    swap_chain_desc1.Stereo = FALSE;
    swap_chain_desc1.SampleDesc.Quality = 0;
    swap_chain_desc1.Flags = 0;
    swap_chain_desc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;


    auto hr = factory->CreateSwapChainForHwnd(
        cmd_queue,
        wnd_handle,
        &swap_chain_desc1,
        nullptr,
        nullptr,
        &swap_chain
    );

    SafeRelease(factory);

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create Swap Chain");
    }

    return swap_chain;
}

void wait_for_gpu()
{
    const UINT64 fence = g.fence_value;
    g.cmd_queue->Signal(g.fence, fence);
    g.fence_value++;

    if (g.fence->GetCompletedValue() < fence)
    {
        g.fence->SetEventOnCompletion(fence, g.fence_event);
        WaitForSingleObject(g.fence_event, INFINITE);
    }
}
