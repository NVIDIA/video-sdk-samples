/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "nvEncDx11App.h"
#include <memory>
#include <cassert>
#include <iostream>

using namespace Microsoft::WRL;
using namespace nvEncBroadcastApi;
//-------------------------------------------------------------------
// CNvEncDx11Interop::CNvEncDx11Interop
//-------------------------------------------------------------------
CNvEncDx11Interop::CNvEncDx11Interop()
    : m_pDevice(nullptr)
    , m_pContext(nullptr)
    , m_pFactory(nullptr)
    , m_pAdapter(nullptr)
    , m_pTexSysMem(nullptr)
    , m_pTexSharedMem(nullptr)
    , m_sharedHandle(nullptr)
{
    m_dxContextType = DxContextType::eDx11Type;
} //constructor

//-------------------------------------------------------------------
// CNvEncDx11Interop::~CNvEncDx11Interop
//-------------------------------------------------------------------
CNvEncDx11Interop::~CNvEncDx11Interop()
{

} //destructor

//-------------------------------------------------------------------
// CNvEncDx11Interop::Initialize
//-------------------------------------------------------------------
bool CNvEncDx11Interop::Initialize(const NVENC_EncodeInitParams* pVideoInfo)
{
    DXGI_FORMAT d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)(&m_pFactory));
    if (!SUCCEEDED(hr) || !m_pFactory)
    {
        return false;
    }

    hr = m_pFactory->EnumAdapters(0, &m_pAdapter);
    if (!SUCCEEDED(hr) || !m_pAdapter)
    {
        return false;
    }

    hr = D3D11CreateDevice(m_pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION, &m_pDevice, NULL, &m_pContext);
    if (!SUCCEEDED(hr) || !m_pDevice)
    {
        return false;
    }

    DXGI_ADAPTER_DESC adapterDesc;
    m_pAdapter->GetDesc(&adapterDesc);
    char szDesc[80];
    wcstombs(szDesc, adapterDesc.Description, sizeof(szDesc));

    if (SUCCEEDED(hr))
    {
        switch (pVideoInfo->bufferFormat)
        {
        case kBufferFormat_ARGB:
            d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        case kBufferFormat_NV12:
            d3dFormat = DXGI_FORMAT_NV12;
            break;
        default:
            d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        }

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = pVideoInfo->width;
        desc.Height = pVideoInfo->height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = d3dFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = m_pDevice->CreateTexture2D(&desc, NULL, &m_pTexSysMem);
        if (!SUCCEEDED(hr) || !m_pTexSysMem)
        {
            return false;
        }

        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = pVideoInfo->width;
        desc.Height = pVideoInfo->height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = d3dFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = 0;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        desc.CPUAccessFlags = 0;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        hr = m_pDevice->CreateTexture2D(&desc, NULL, &m_pTexSharedMem);
        if (!SUCCEEDED(hr) || !m_pTexSharedMem)
        {
            return false;
        }
#if 0
        // Create the render target view.
        // Setup the description of the render target views.
        D3D11_RENDER_TARGET_VIEW_DESC stepCountRenderTargetViewDesc;
        stepCountRenderTargetViewDesc.Format = desc.Format;
        stepCountRenderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        stepCountRenderTargetViewDesc.Texture2D.MipSlice = 0;
        hr = m_pDevice->CreateRenderTargetView(m_pTexSharedMem, &stepCountRenderTargetViewDesc, &m_pRTV);
        if (FAILED(hr))
        {
            //goto error_exit;
            return false;
        }
#endif

        // QI IDXGIResource interface to synchronized shared surface.
         ComPtr<IDXGIResource> pDXGIResource = nullptr;
        hr = m_pTexSharedMem.Get()->QueryInterface(__uuidof(IDXGIResource), (LPVOID*) &pDXGIResource);
        if (FAILED(hr) || !pDXGIResource)
        {
            return false;
        }

        // obtain handle to IDXGIResource object.
        hr = pDXGIResource->GetSharedHandle(&m_sharedHandle);
        if (FAILED(hr) || m_sharedHandle == nullptr)
        {
            return false;
        }
        return true;
    }
    return false;
} //Initialize

//-------------------------------------------------------------------
// CNvEncDx11Interop::SetFrameParams
//-------------------------------------------------------------------
bool CNvEncDx11Interop::SetFrameParams(nvEncBroadcastApi::NVENC_EncodeInfo& encodeInfo, const nvEncBroadcastApi::NVENC_EncodeInitParams& initParams, const uint8_t* pSrc)
{
    HRESULT hr = S_OK;

    if (!pSrc)
    {
        return false;
    }

    if (m_sharedHandle == nullptr) //system memory case
    {
        encodeInfo.bufferInfo.bufferType = kBufferType_Sys;
        encodeInfo.bufferInfo.bufferFormat = initParams.bufferFormat;
        encodeInfo.bufferInfo.SysBuffer.lineWidth = initParams.width;
        encodeInfo.bufferInfo.SysBuffer.pixelBuffer = reinterpret_cast<uint64_t>(pSrc);
        return true;
    }

    ComPtr<IDXGIKeyedMutex> pDXGIKeyedMutex = nullptr;

    // QI IDXGIKeyedMutex interface of synchronized shared surface's resource handle.
    if (!pDXGIKeyedMutex.Get())
    {
        hr = m_pTexSharedMem.Get()->QueryInterface(__uuidof(IDXGIKeyedMutex),
            &pDXGIKeyedMutex);

        if (FAILED(hr) || (pDXGIKeyedMutex.Get() == nullptr))
        {
            goto exit;
        }
    }
    //test the key mutex
    DWORD timeOut = 5000;
    DWORD result = pDXGIKeyedMutex->AcquireSync(DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY, timeOut);
    if (result != WAIT_OBJECT_0)
    {
        assert(0);
    }

    D3D11_MAPPED_SUBRESOURCE map;
    hr = m_pContext->Map(m_pTexSysMem.Get(), 0, D3D11_MAP_WRITE, 0, &map);
    if (SUCCEEDED(hr))
    {
        switch (initParams.bufferFormat)
        {
            case kBufferFormat_ARGB:
            {
                for (unsigned int y = 0; y < initParams.height; y++)
                {
                    uint64_t dstDataPtr = (uint64_t)(map.pData) + y * map.RowPitch;
                    uint64_t srcDataPtr = (uint64_t)(pSrc) + y * initParams.width * 4;
                    memcpy((uint8_t*)(dstDataPtr), (uint8_t *)(srcDataPtr), initParams.width * 4);
                }
            }
            break;

            case kBufferFormat_NV12:
            {
                for (unsigned int y = 0; y < initParams.height; y++)
                {
                    memcpy((uint8_t *)map.pData + y * map.RowPitch, (uint8_t *)(pSrc + y * initParams.width),initParams.width * 1);
                }

                uint8_t* pUVDest = reinterpret_cast<uint8_t*>((uint64_t)(map.pData) + map.RowPitch * initParams.height);
                uint8_t* pSrcPtr = reinterpret_cast<uint8_t*>((uint64_t)(pSrc) + initParams.height * initParams.width * 1);
                for (unsigned int y = 0; y < initParams.height / 2; y++)
                {
                    memcpy((uint8_t *)pUVDest, pSrcPtr, initParams.width);
                    pUVDest += map.RowPitch;
                    pSrcPtr += initParams.width;
                }
            }
            break;
        }
        m_pContext->Unmap(m_pTexSysMem.Get(), 0);
        m_pContext->Flush();

        D3D11_TEXTURE2D_DESC desc = { 0 };
        D3D11_TEXTURE2D_DESC desc2 = { 0 };
        //copy the surface
        memset(&desc, 0, sizeof(desc));
        m_pTexSysMem->GetDesc(&desc);
        m_pTexSharedMem->GetDesc(&desc2);

        if (desc.Format != desc2.Format)
        {
            assert(0);
        }

        D3D11_BOX stBox;
        memset(&stBox, 0, sizeof(stBox));

        stBox.left = 0;
        stBox.top = 0;
        stBox.front = 0;
        stBox.right = desc.Width-1;
        stBox.bottom = desc.Height-1;
        stBox.back = 1;
        D3D11_BOX* pBox = &stBox;

        ComPtr<ID3D11DeviceContext> pImmediateContext = nullptr;
        m_pDevice->GetImmediateContext(&pImmediateContext);
        if (!pImmediateContext.Get())
        {
            goto exit;
        }
        pImmediateContext->CopySubresourceRegion(m_pTexSharedMem.Get(), 0, 0, 0, 0, m_pTexSysMem.Get(), 0, pBox);
        pImmediateContext->CopyResource(m_pTexSharedMem.Get(), m_pTexSysMem.Get());
        pImmediateContext->Flush();

        // Handle unable to acquire shared surface error.
        if (pDXGIKeyedMutex.Get())
        {
            result = pDXGIKeyedMutex->ReleaseSync(DXSURFACE_CREATORRELEASE_USERACQUIRE_KEY);
            if (!SUCCEEDED(result))
            {
                assert(0);
                std::cout << "ReleaseSync failed with an error, nvEnc might not be able to access the surface" << std::endl;
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        encodeInfo.bufferInfo.bufferType = kBufferType_Vid;
        encodeInfo.bufferInfo.bufferFormat = initParams.bufferFormat;
        encodeInfo.bufferInfo.DxBuffer.bufferHandle = m_sharedHandle;
    }
exit:
    return SUCCEEDED(hr) ? true : false;
} //Encode

//-------------------------------------------------------------------
// CNvEncDx11Interop::ReleaseNvEnc
//-------------------------------------------------------------------
bool CNvEncDx11Interop::ReleaseNvEnc()
{
    return true;
} //ReleaseNvEnc