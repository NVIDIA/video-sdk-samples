/*
* Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/
#pragma once

#include <iostream>
#include <mutex>
#include <thread>
#include <d3d11.h>
#include <cuda.h>
#include <cudaD3D11.h>
#include "FramePresenterD3D.h"
#include "../Utils/NvCodecUtils.h"

class FramePresenterD3D11 : public FramePresenterD3D
{
public:
    FramePresenterD3D11(CUcontext cuContext, int nWidth, int nHeight) : 
        FramePresenterD3D(cuContext, nWidth, nHeight) 
    {
        pthMsgLoop = new std::thread(ThreadProc, this);
        while (!bReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    ~FramePresenterD3D11() {
        bQuit = true;
        pthMsgLoop->join();
        delete pthMsgLoop;
    }

    bool PresentHostFrame(BYTE *pData, int nBytes) {
        mtx.lock();
        if (!bReady) {
            mtx.unlock();
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mappedTexture;
        ck(pContext->Map(pStagingTexture, 0, D3D11_MAP_WRITE, 0, &mappedTexture));
        memcpy(mappedTexture.pData, pData, min(nWidth * nHeight * 4, nBytes));
        pContext->Unmap(pStagingTexture, 0);
        pContext->CopyResource(pBackBuffer, pStagingTexture);
        ck(pSwapChain->Present(0, 0));
        mtx.unlock();
        return true;
    }

    bool PresentDeviceFrame(unsigned char *dpBgra, int nPitch) {
        mtx.lock();
        if (!bReady) {
            mtx.unlock();
            return false;
        }
        CopyDeviceFrame(dpBgra, nPitch);
        ck(pSwapChain->Present(0, 0));
        mtx.unlock();
        return true;
    }

private:
    static void ThreadProc(FramePresenterD3D11 *This) {
        This->Run();
    }

    void Run() {
        HWND hwndMain = CreateAndShowWindow(nWidth, nHeight);

        DXGI_SWAP_CHAIN_DESC sc = { 0 };
        sc.BufferCount = 1;
        sc.BufferDesc.Width = nWidth;
        sc.BufferDesc.Height = nHeight;
        sc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.BufferDesc.RefreshRate.Numerator = 0;
        sc.BufferDesc.RefreshRate.Denominator = 1;
        sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc.OutputWindow = hwndMain;
        sc.SampleDesc.Count = 1;
        sc.SampleDesc.Quality = 0;
        sc.Windowed = TRUE;

        ID3D11Device *pDevice = NULL;
        ck(D3D11CreateDeviceAndSwapChain(GetAdapterByContext(cuContext), D3D_DRIVER_TYPE_UNKNOWN,
            NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sc, &pSwapChain, &pDevice, NULL, &pContext));
        ck(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer));

        D3D11_TEXTURE2D_DESC td;
        pBackBuffer->GetDesc(&td);
        td.BindFlags = 0;
        td.Usage = D3D11_USAGE_STAGING;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ck(pDevice->CreateTexture2D(&td, NULL, &pStagingTexture));

        ck(cuCtxPushCurrent(cuContext));
        ck(cuGraphicsD3D11RegisterResource(&cuResource, pBackBuffer, CU_GRAPHICS_REGISTER_FLAGS_NONE));
        ck(cuGraphicsResourceSetMapFlags(cuResource, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD));
        ck(cuCtxPopCurrent(NULL));

        bReady = true;
        MSG msg = { 0 };
        while (!bQuit && msg.message != WM_QUIT) {
            if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        mtx.lock();
        bReady = false;
        ck(cuCtxPushCurrent(cuContext));
        ck(cuGraphicsUnregisterResource(cuResource));
        ck(cuCtxPopCurrent(NULL));
        pStagingTexture->Release();
        pBackBuffer->Release();
        pContext->Release();
        pDevice->Release();
        pSwapChain->Release();
        DestroyWindow(hwndMain);
        mtx.unlock();
    }

    static IDXGIAdapter *GetAdapterByContext(CUcontext cuContext) {
        CUdevice cuDeviceTarget;
        ck(cuCtxPushCurrent(cuContext));
        ck(cuCtxGetDevice(&cuDeviceTarget));
        ck(cuCtxPopCurrent(NULL));

        IDXGIFactory1 *pFactory = NULL;
        ck(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&pFactory));
        IDXGIAdapter *pAdapter = NULL;
        for (unsigned i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
            CUdevice cuDevice;
            ck(cuD3D11GetDevice(&cuDevice, pAdapter));
            if (cuDevice == cuDeviceTarget) {
                pFactory->Release();
                return pAdapter;
            }
            pAdapter->Release();
        }
        pFactory->Release();
        return NULL;
    }

private:
    bool bReady = false;
    bool bQuit = false;
    std::mutex mtx;
    std::thread *pthMsgLoop = NULL;

    IDXGISwapChain *pSwapChain = NULL;
    ID3D11DeviceContext *pContext = NULL;
    ID3D11Texture2D *pBackBuffer = NULL, *pStagingTexture = NULL;
};
