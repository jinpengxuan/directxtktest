//--------------------------------------------------------------------------------------
// File: AnimTest.cpp
//
// Developer unit test for DirectXTK Model animation
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "CommonStates.h"
#include "Effects.h"
#include "Model.h"
#include "DirectXColors.h"
#include "DirectXPackedVector.h"
#include "ScreenGrab.h"

#pragma warning(push)
#pragma warning(disable : 4005)
#include <wincodec.h>
#pragma warning(pop)

using namespace DirectX;
using namespace DirectX::PackedVector;

// Build for LH vs. RH coords
#define LH_COORDS

struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}


int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow )
{
    HRESULT hr;

    wchar_t *const className = L"TestWindowClass";

    WNDCLASSEX wndClass = { 0 };

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = className;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassEx(&wndClass);

    HWND hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, className, L"Test Window", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 720, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    RECT client;
    GetClientRect(hwnd, &client);

    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11Texture2D* backBufferTexture;
    ID3D11RenderTargetView* backBuffer;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11DepthStencilView* depthStencil;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = client.right;
    swapChainDesc.BufferDesc.Height = client.bottom;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_10_0;
    
    DWORD d3dFlags = 0;
#ifdef _DEBUG
    d3dFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    if (FAILED(hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, d3dFlags, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &device, NULL, &context)))
        return 1;

    if (FAILED(hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBufferTexture)))
        return 1;

    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = { DXGI_FORMAT_UNKNOWN, D3D11_RTV_DIMENSION_TEXTURE2D };

    if (FAILED(hr = device->CreateRenderTargetView(backBufferTexture, &renderTargetViewDesc, &backBuffer)))
        return 1;

    D3D11_TEXTURE2D_DESC depthStencilDesc = { 0 };

    depthStencilDesc.Width = client.right;
    depthStencilDesc.Height = client.bottom;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;

    if (FAILED(device->CreateTexture2D(&depthStencilDesc, NULL, &depthStencilTexture)))
        return 1;

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    ZeroMemory(&depthStencilViewDesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
    depthStencilViewDesc.Format = DXGI_FORMAT_UNKNOWN;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    if (FAILED(device->CreateDepthStencilView(depthStencilTexture, &depthStencilViewDesc, &depthStencil)))
        return 1;

    CommonStates states(device);
    EffectFactory fx(device);

#ifdef LH_COORDS
    bool ccw = false;
#else
    bool ccw = true;
#endif

    std::unique_ptr<XMMATRIX[], aligned_deleter> bones(
        reinterpret_cast<XMMATRIX*>( _aligned_malloc( sizeof(XMMATRIX) * SkinnedEffect::MaxBones, 16 ) ) );

    XMMATRIX id = XMMatrixIdentity();
    for( size_t j=0; j < SkinnedEffect::MaxBones; ++j )
    {
        bones[ j ] = id;
    }

    // VS 2012 CMO
    auto teapot = Model::CreateFromCMO( device, L"teapot.cmo", fx, ccw, false );

    // DirectX SDK Mesh
    auto tank = Model::CreateFromSDKMESH( device, L"TankScene.sdkmesh", fx, !ccw );

    auto soldier = Model::CreateFromSDKMESH( device, L"soldier.sdkmesh", fx, !ccw, false );

    bool quit = false;

    D3D11_VIEWPORT vp = { 0, 0, (float)client.right, (float)client.bottom, 0, 1 };

    context->RSSetViewports(1, &vp);

    context->OMSetRenderTargets(1, &backBuffer, depthStencil);

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);

    size_t frame = 0;

    context->OMSetDepthStencilState( states.DepthDefault(), 0 );
    context->OMSetBlendState( states.Opaque(), nullptr, 0xFFFFFFFF );

    while (!quit)
    {
        MSG msg;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                quit = true;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        
        float time = (float)(counter.QuadPart - start.QuadPart) / (float)freq.QuadPart;

        context->ClearRenderTargetView(backBuffer, Colors::CornflowerBlue);
        context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

        // Compute camera matrices.
        float alphaFade = (sin(time * 2) + 1) / 2;

        if (alphaFade >= 1)
            alphaFade = 1 - FLT_EPSILON;

        float yaw = time * 0.4f;
        float pitch = time * 0.7f;
        float roll = time * 1.1f;

        XMVECTORF32 cameraPosition = { 0, 0, 6 };

        float aspect = (float)client.right / (float)client.bottom;

        XMMATRIX world = XMMatrixRotationY( XM_PI );

#ifdef LH_COORDS
        XMMATRIX view = XMMatrixLookAtLH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
        XMMATRIX projection = XMMatrixPerspectiveFovLH(1, aspect, 1, 15);
#else
        XMMATRIX view = XMMatrixLookAtRH(cameraPosition, g_XMZero, XMVectorSet(0, 1, 0, 0));
        XMMATRIX projection = XMMatrixPerspectiveFovRH(1, aspect, 1, 15);
#endif

        const float row0 = 2.f;
        const float row1 = 0.f; 
        const float row2 = -2.f;

            // Skinning settings
        float s = 1 + sin(time * 1.7f) * 0.5f;

        XMMATRIX scale = XMMatrixScaling(s,s,s);

        for (size_t j=0; j < SkinnedEffect::MaxBones; ++j )
        {
            bones[ j ] = scale;
        }

        // Draw CMO models
        XMMATRIX local = XMMatrixMultiply( XMMatrixScaling( 0.3f, 0.3f, 0.3f ), XMMatrixTranslation( 0.f, row2, 0.f ) );
        local = XMMatrixMultiply( world, local );
        tank->Draw( context, states, local, view, projection );

        teapot->UpdateEffects([&](IEffect* effect)
        {
            auto skinnedEffect = dynamic_cast<IEffectSkinning*>( effect );
            if ( skinnedEffect )
                skinnedEffect->ResetBoneTransforms();
        });
        local = XMMatrixMultiply( XMMatrixScaling( 0.01f, 0.01f, 0.01f ), XMMatrixTranslation( -2.f, row0, 0.f ) );
        teapot->Draw( context, states, local, view, projection );

        teapot->UpdateEffects([&](IEffect* effect)
        {
            auto skinnedEffect = dynamic_cast<IEffectSkinning*>( effect );
            if ( skinnedEffect )
                skinnedEffect->SetBoneTransforms(bones.get(), SkinnedEffect::MaxBones);
        });
        local = XMMatrixMultiply( XMMatrixScaling( 0.01f, 0.01f, 0.01f ), XMMatrixTranslation( -2.f, row1, 0.f ) );
        teapot->Draw( context, states, local, view, projection );

        // Draw SDKMESH models
        soldier->UpdateEffects([&](IEffect* effect)
        {
            auto skinnedEffect = dynamic_cast<IEffectSkinning*>( effect );
            if ( skinnedEffect )
                skinnedEffect->ResetBoneTransforms();
        });
        local = XMMatrixMultiply( XMMatrixScaling( 2.f, 2.f, 2.f ), XMMatrixTranslation( 2.f, row0, 0.f ) );
        local = XMMatrixMultiply( world, local );
        soldier->Draw( context, states, local, view, projection );

        soldier->UpdateEffects([&](IEffect* effect)
        {
            auto skinnedEffect = dynamic_cast<IEffectSkinning*>( effect );
            if ( skinnedEffect )
                skinnedEffect->SetBoneTransforms(bones.get(), SkinnedEffect::MaxBones);
        });
        local = XMMatrixMultiply( XMMatrixScaling( 2.f, 2.f, 2.f ), XMMatrixTranslation( 2.f, row1, 0.f ) );
        local = XMMatrixMultiply( world, local );
        soldier->Draw( context, states, local, view, projection );

        swapChain->Present(1, 0);
        ++frame;

        if ( frame == 10 )
        {
            ID3D11Texture2D* backBufferTex = nullptr;
            hr = swapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&backBufferTex);
            if ( SUCCEEDED(hr) )
            {
                hr = SaveWICTextureToFile( context, backBufferTex, GUID_ContainerFormatBmp, L"SCREENSHOT.BMP" );
                hr = SaveDDSTextureToFile( context, backBufferTex, L"SCREENSHOT.DDS" );
                backBufferTex->Release();
            }
        }

        time++;
    }

    depthStencilTexture->Release();
    depthStencil->Release();
    backBuffer->Release();
    backBufferTexture->Release();
    swapChain->Release();
    context->Release();
    device->Release();

    return 0;
}
