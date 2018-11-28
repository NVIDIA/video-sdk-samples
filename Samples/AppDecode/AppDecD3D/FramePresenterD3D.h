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
#include <cuda.h>
#include "../Utils/NvCodecUtils.h"

class FramePresenterD3D {
public:
    FramePresenterD3D(CUcontext cuContext, int nWidth, int nHeight) : cuContext(cuContext), nWidth(nWidth), nHeight(nHeight) {}
    virtual ~FramePresenterD3D() {};
    virtual bool PresentDeviceFrame(unsigned char *dpBgra, int nPitch) = 0;

protected:
    static HWND CreateAndShowWindow(int nWidth, int nHeight) {
        double r = max(nWidth / 1280.0, nHeight / 720.0);
        if (r > 1.0) {
            nWidth = (int)(nWidth / r);
            nHeight = (int)(nHeight / r);
        }

        static char szAppName[] = "D3DPresenter";
        WNDCLASS wndclass;
        wndclass.style = CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc = WndProc;
        wndclass.cbClsExtra = 0;
        wndclass.cbWndExtra = 0;
        wndclass.hInstance = (HINSTANCE)GetModuleHandle(NULL);
        wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wndclass.lpszMenuName = NULL;
        wndclass.lpszClassName = szAppName;
        RegisterClass(&wndclass);

        RECT rc{
            (GetSystemMetrics(SM_CXSCREEN) - nWidth) / 2,
            (GetSystemMetrics(SM_CYSCREEN) - nHeight) / 2,
            (GetSystemMetrics(SM_CXSCREEN) + nWidth) / 2,
            (GetSystemMetrics(SM_CYSCREEN) + nHeight) / 2
        };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwndMain = CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, wndclass.hInstance, NULL);
        ShowWindow(hwndMain, SW_SHOW);
        UpdateWindow(hwndMain);

        return hwndMain;
    }

    void CopyDeviceFrame(unsigned char *dpBgra, int nPitch) {
        ck(cuCtxPushCurrent(cuContext));
        ck(cuGraphicsMapResources(1, &cuResource, 0));
        CUarray dstArray;
        ck(cuGraphicsSubResourceGetMappedArray(&dstArray, cuResource, 0, 0));

        CUDA_MEMCPY2D m = { 0 };
        m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        m.srcDevice = (CUdeviceptr)dpBgra;
        m.srcPitch = nPitch ? nPitch : nWidth * 4;
        m.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        m.dstArray = dstArray;
        m.WidthInBytes = nWidth * 4;
        m.Height = nHeight;
        ck(cuMemcpy2D(&m));

        ck(cuGraphicsUnmapResources(1, &cuResource, 0));
        ck(cuCtxPopCurrent(NULL));
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

protected:
    int nWidth = 0, nHeight = 0;
    CUcontext cuContext = NULL;
    CUgraphicsResource cuResource = NULL;
};
