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

#include <cassert>
#include <dxgi.h>
#include <vector>
#include <iostream>
#include <wincodec.h>
#include "nvEncBroadcastInterfaceImplD3D11.h"
#include "nvEncBroadcastBitstreamBuffer.h"
#include "ScopeGuard.h"
#include "nvEncBroadcastUtils.h"

using namespace nvEncBroadcastApi;
//#define DUMP_TEXTURE
//#define TEST_CLEARRECT

namespace
{
}

nvEncBroadcastInterfaceImplD3D11::nvEncBroadcastInterfaceImplD3D11()
           : INvEncBroadcastInterface()
           , m_bNvEncInterfaceInitialized(false)
           , m_d3dFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
           , m_RefCount(0)
           , m_ulEncodeBuffers(MAX_ENCODE_BUFFERS)
           , m_bConvertToNV12(false)
           , m_frameCnt(0)

{
    m_pEncodeD3D11.reset();
    memset(&m_createParams, 0, sizeof(m_createParams));
    m_surfaceHandleMap.clear();
    ++m_RefCount;
} //Constructor

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::~nvEncBroadcastInterfaceImplD3D11
//-------------------------------------------------------------------
nvEncBroadcastInterfaceImplD3D11::~nvEncBroadcastInterfaceImplD3D11()
{
    ReleaseResources();
} //Destructor

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::ReleaseResources
//-------------------------------------------------------------------
void nvEncBroadcastInterfaceImplD3D11::ReleaseResources()
{
    ID3D11Texture2D* pTexture = nullptr;
    m_pEncodeD3D11->UnregisterResources();

    SurfaceHandleMap::const_iterator next_it;
    for (auto it = m_surfaceHandleMap.cbegin(), next_it = m_surfaceHandleMap.cbegin(); it != m_surfaceHandleMap.cend(); it = next_it)
    {
        HANDLE surfaceHandle = reinterpret_cast<HANDLE>((*it).first);
        ID3D11Texture2D* pTex11Shared = reinterpret_cast<ID3D11Texture2D*>((*it).second);
        if (pTex11Shared)
        {
            DBGMSG(dbgINFO, L"%s - Cached texture - %x with %x handle being released", WFUNCTION, pTex11Shared, surfaceHandle);
            pTex11Shared->Release();
            pTex11Shared = nullptr;
        }
        m_pContext->Flush();
        next_it = it; ++next_it;
        {
            m_surfaceHandleMap.erase(it);
        }
    }

    for (unsigned int i = 0; i < m_ulEncodeBuffers; i++)
    {
        pTexture =  m_pInputTexture[i].release();
        if (pTexture)
        {
            pTexture->Release();
            pTexture = nullptr;
        }
    }
} //ReleaseResources

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::AllocateInputBuffers
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::AllocateInputBuffers(uint32_t numInputBuffers, NV_ENC_BUFFER_FORMAT nvEncFormat)
{
    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = S_OK;
    DXGI_FORMAT d3dFormat = DXGI_FORMAT_UNKNOWN;

    switch (nvEncFormat)
    {
    case NV_ENC_BUFFER_FORMAT_ARGB:
        d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case NV_ENC_BUFFER_FORMAT_ABGR:
        d3dFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case NV_ENC_BUFFER_FORMAT_NV12_PL:
        d3dFormat = DXGI_FORMAT_NV12;
        break;
    default:
        break;
    }

    if (d3dFormat == DXGI_FORMAT_UNKNOWN)
    {
        DBGMSG(dbgERROR, L"%s - Invalid format %x, returning error", WFUNCTION, d3dFormat);
        return E_INVALIDARG;
    }

    std::vector<void*> inputFrames;
    for (unsigned int i = 0; i < m_ulEncodeBuffers; i++)
    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = m_encodeParams.width;
        desc.Height = m_encodeParams.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = d3dFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        hr = m_pDevice->CreateTexture2D(&desc, NULL, &pTexture);
        if (!SUCCEEDED(hr))
        {
            DBGMSG(dbgERROR, L"%s - Allocating %d D3D surface for nvEnc input queue failed, error - %x", WFUNCTION, i, hr);
            break;
        }
        inputFrames.push_back(pTexture);
        m_pInputTexture[i].reset(pTexture);
    }
    if (SUCCEEDED(hr))
    {
        m_pEncodeD3D11.get()->RegisterResources(inputFrames, NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX,
            m_encodeParams.width, m_encodeParams.height, 0, nvEncFormat, false);
    }
    if (!SUCCEEDED(hr))
    {
        for (unsigned int i = 0; i < numInputBuffers; i++)
        {
            pTexture =  m_pInputTexture[i].release();
            if (pTexture)
            {
                pTexture->Release();
                pTexture = nullptr;
            }
        }
    }
    return hr; 
} //AllocateInputBuffers

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::InitializeDxVA
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::InitializeDxVA()
{
    HRESULT hr = S_OK;
    if (!m_pDevice.Get())
    {
        return E_NOT_SET;
    }

    hr = m_pDevice.As(&m_pVideoDevice);
    if (!SUCCEEDED(hr) || !m_pVideoDevice.Get())
    {
        DBGMSG(dbgERROR, L"%s - QueryInterface on m_pDevice for ID3D11VideoDevice failed with error - %d\n", WFUNCTION, hr);
        return E_FAIL;
    }

    hr = m_pContext.As(&m_pVideoContext);
    if (!SUCCEEDED(hr) || !m_pVideoContext.Get())
    {
        DBGMSG(dbgERROR, L"%s - QueryInterface on m_pContext for ID3D11VideoContext failed with error - %d\n", WFUNCTION, hr);
        return E_FAIL;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = 
    {
        D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
        { 1, 1 }, m_encodeParams.width, m_encodeParams.height,
        { 1, 1 }, m_encodeParams.width, m_encodeParams.height,
        D3D11_VIDEO_USAGE_PLAYBACK_NORMAL
    };

    hr = m_pVideoDevice->CreateVideoProcessorEnumerator(&contentDesc, m_pVideoProcessorEnumerator.GetAddressOf());
    if (!SUCCEEDED(hr) || !m_pVideoProcessorEnumerator.Get())
    {
        DBGMSG(dbgERROR, L"%s - CreateVideoProcessorEnumerator failed with error - %d\n", WFUNCTION, hr);
        return E_FAIL;
    }

    hr = m_pVideoDevice->CreateVideoProcessor(m_pVideoProcessorEnumerator.Get(), 0, m_pVideoProcessor.GetAddressOf());
    if (!SUCCEEDED(hr) || !m_pVideoProcessor.Get())
    {
        DBGMSG(dbgERROR, L"%s - CreateVideoProcessor failed with error - %d\n", WFUNCTION, hr);
        return E_FAIL;
    }
    return S_OK;
} //InitializeDxVA

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::Initialize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::Initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    HRESULT hr = S_OK;
    NV_ENC_BUFFER_FORMAT nvEncFormat;
    bool bDxVAInit = false;

    if (!pEncodeParams)
    {
        DBGMSG(dbgERROR, L"%s - Null pointer to pEncodeParams passed, returning error", WFUNCTION);
        return E_INVALIDARG;
    }
    DBGMSG(dbgINFO, L"%s - Using DX11 path for encode, width - %x, height - %x, format - %x", WFUNCTION, pEncodeParams->width, pEncodeParams->height, pEncodeParams->bufferFormat);

    ScopeGuards cleanup;
    auto cleanupFunc = [&, this]()
    { 
        if (SUCCEEDED(hr))
        {
            this->m_bNvEncInterfaceInitialized = true;
        }
    };

    cleanup += cleanupFunc;
    memcpy(&m_createParams, pEncodeParams, sizeof(NVENC_EncodeInitParams));

    if (!m_bNvEncInterfaceInitialized)
    {
        hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)(m_pFactory.GetAddressOf()));
        if (!SUCCEEDED(hr) || !m_pFactory.Get())
        {
            DBGMSG(dbgERROR, L"%s: CreateDXGIFactory1 failed with %x error", WFUNCTION, hr);
            return hr;
        }

        hr = m_pFactory->EnumAdapters(0, m_pAdapter.GetAddressOf());
        if (!SUCCEEDED(hr) || !m_pAdapter.Get())
        {
            DBGMSG(dbgERROR, L"%s: EnumAdapters failed with %x error", WFUNCTION, hr);
            return hr;
        }

        hr = D3D11CreateDevice(m_pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
            NULL, 0, D3D11_SDK_VERSION, m_pD3DDevice.GetAddressOf(), NULL, m_pD3DDeviceContext.GetAddressOf());
        if (!SUCCEEDED(hr) || !m_pD3DDevice.Get())
        {
            DBGMSG(dbgERROR, L"%s: D3D11CreateDevice failed with %x error", WFUNCTION, hr);
            return hr;
        }

        hr = m_pD3DDevice.As(&m_pDevice);
        if (!SUCCEEDED(hr) || !m_pDevice.Get())
        {
            DBGMSG(dbgERROR, L"%s: ID3D11Device1 not retrieved, %x error", WFUNCTION, hr);
            return hr;
        }

        hr = m_pD3DDeviceContext.As(&m_pContext);
        if (!SUCCEEDED(hr) || !m_pContext.Get())
        {
            DBGMSG(dbgERROR, L"%s: ID3D11DeviceContext1 not retieved, %x error", WFUNCTION, hr);
            return hr;
        }

        DXGI_ADAPTER_DESC adapterDesc;
        m_pAdapter->GetDesc(&adapterDesc);
        DBGMSG(dbgPROFILE, L"%s: GPU in use - %s", WFUNCTION, adapterDesc.Description);
        if (SUCCEEDED(hr))
        {
            memcpy(&m_encodeParams, pEncodeParams, sizeof(m_encodeParams));

            switch (m_encodeParams.bufferFormat)
            {
            case kBufferFormat_ARGB:
                m_d3dFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                if (m_bConvertToNV12)
                {
                    bDxVAInit = true;
                    nvEncFormat = NV_ENC_BUFFER_FORMAT_NV12_PL;
                }
                else
                {
                    nvEncFormat = NV_ENC_BUFFER_FORMAT_ARGB;
                }
                break;
            case kBufferFormat_ABGR:
                m_d3dFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                if (m_bConvertToNV12)
                {
                    bDxVAInit = true;
                    nvEncFormat = NV_ENC_BUFFER_FORMAT_NV12_PL;
                }
                else
                {
                    nvEncFormat = NV_ENC_BUFFER_FORMAT_ABGR;
                }
                break;
            case kBufferFormat_NV12:
                bDxVAInit = true;
                m_d3dFormat = DXGI_FORMAT_NV12;
                nvEncFormat = NV_ENC_BUFFER_FORMAT_NV12_PL;
                break;
            default:
                hr = E_INVALIDARG;
                break;
            }

            if (!SUCCEEDED(hr))
            {
                DBGMSG(dbgERROR, L"%s: Invalid %x buffer format passed, returning error", WFUNCTION, m_encodeParams.bufferFormat);
                return hr;
            }
            DBGMSG(dbgINFO, L"%s D3D Format - %d nvEnc format - %d retrieved", WFUNCTION, m_d3dFormat, nvEncFormat);

            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
            desc.Width = m_encodeParams.width;
            desc.Height = m_encodeParams.height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = m_d3dFormat;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#ifdef DUMP_TEXTURE
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
#endif
            hr = m_pDevice->CreateTexture2D(&desc, NULL, m_pTexStagingMem.GetAddressOf());
            if (!SUCCEEDED(hr) || !m_pTexStagingMem.Get())
            {
                DBGMSG(dbgERROR, L"%s: CreateTexture2D failed with %x error", WFUNCTION, hr);
                return hr;
            }
        }

        if (SUCCEEDED(hr) && bDxVAInit)
        {
            hr = InitializeDxVA();
            if (!SUCCEEDED(hr) || !m_pVideoProcessor.Get())
            {
                DBGMSG(dbgERROR, L"%s: InitializeDxVA failed with %x error", WFUNCTION, hr);
                return hr;
            }
        }

        if (SUCCEEDED(hr))
        {
            try
            {
                m_pEncodeD3D11.reset(new NvEncoderD3D11(m_pDevice.Get(), m_encodeParams.width, m_encodeParams.height, nvEncFormat, EXTRA_BUFFERS_DELAY_NVENC));
                if (!m_pEncodeD3D11.get())
                {
                    DBGMSG(dbgERROR, L"%s: NvEncoderD3D11 failed ", WFUNCTION);
                    hr = E_FAIL;
                }
            }
            catch (std::exception& e)
            {
                DBGMSG(dbgERROR, L"%s: creating NvEncoderD3D11 object threw exception : %s ", WFUNCTION, e.what());
                hr = E_FAIL;
            }
            catch (...)
            {
                DBGMSG(dbgERROR, L"%s: creating NvEncoderD3D11 object threw unhandled exception", WFUNCTION);
                hr = E_FAIL;
            }

            if (SUCCEEDED(hr))
            {
                try
                {
                    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
                    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
                    initializeParams.encodeConfig = &encodeConfig;
                    hr = INvEncBroadcastInterface::initializeEncodeConfig(m_pEncodeD3D11.get(), &initializeParams, pEncodeSettingsParams);
                    if (!SUCCEEDED(hr))
                    {
                        DBGMSG(dbgERROR, L"%s - initializing encode config params failed with error - %x\n", WFUNCTION, hr);
                        return hr;
                    }
                    m_pEncodeD3D11.get()->CreateEncoder(&initializeParams, false);
                }
                catch (std::exception& e)
                {
                    DBGMSG(dbgERROR, L"%s: Initializing NvEncoderD3D11 with params threw exception : %s ", WFUNCTION, e.what());
                    hr = E_FAIL;
                    goto fail;
                }
                catch (...)
                {
                    DBGMSG(dbgERROR, L"%s: Initializing NvEncoderD3D11 with params threw unhandled exception", WFUNCTION);
                    hr = E_FAIL;
                    goto fail;
                }
            }
        }

        if (SUCCEEDED(hr))
        {
            m_ulEncodeBuffers = m_pEncodeD3D11.get()->GetOutputPreferredBuffers();
            DBGMSG(dbgINFO, L"%s - GetOutputPreferredBuffers returned %d buffers, creating as many buffers with DX format - %d", WFUNCTION, m_ulEncodeBuffers, m_d3dFormat);
            hr = AllocateInputBuffers(m_ulEncodeBuffers, nvEncFormat);
        }
    }

    if (SUCCEEDED(hr))
    {
        DBGMSG(dbgPROFILE, L"%s: NvEnc successfully initialized", WFUNCTION);
        return hr;
    }
fail:
    NvEncoderD3D11* pEncoder = m_pEncodeD3D11.release();
    if (pEncoder)
    {
        pEncoder->DestroyEncoder();
        delete pEncoder;
        pEncoder = nullptr;
    }
    return hr;
} //Initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::Release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::Release()
{
    --m_RefCount;

    assert(m_RefCount == 0);
    delete this;

    return API_SUCCESS;
} //Release

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::CopySharedTexture
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::CopySharedTexture(ID3D11DeviceContext* pContext, ID3D11Texture2D* pTexDest, ID3D11Texture2D* pTex11Shared, DXGI_FORMAT d3dFormat)
{
    HRESULT hr = S_OK;
    if (!pTexDest || !pTex11Shared)
    {
        DBGMSG(dbgINFO, L"%s - Null pointer to either source texture - %x or destination texture - %x passed", WFUNCTION, pTexDest, pTex11Shared);
        return E_INVALIDARG;
    }

    D3D11_TEXTURE2D_DESC desc;
    pTex11Shared->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC desc2;
    pTexDest->GetDesc(&desc2);

    if (!m_bConvertToNV12)
    {
        if (desc.Format != desc2.Format)
        {
            DBGMSG(dbgERROR, L"%s - Shared Texture - Format - %x ...Destination - Format - %x", WFUNCTION, desc.Format, desc2.Format);
            assert(0);
            return E_INVALIDARG;
        }
    }
    switch (d3dFormat)
    {
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        {
            if (!m_bConvertToNV12)
            {
#ifdef TEST_CLEARRECT
                Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pTargetView;
                D3D11_RENDER_TARGET_VIEW_DESC rtv;
                rtv.Format = d3dFormat;
                rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv.Texture2D.MipSlice  = 0;

                hr = m_pDevice->CreateRenderTargetView(pTexDest, &rtv, &pTargetView);
                if (!SUCCEEDED(hr) || !pTargetView.Get())
                {
                    DBGMSG(dbgERROR, L"%s - CreateRenderTargetView failed with %x error", WFUNCTION, hr);
                }
#endif
                D3D11_BOX stBox;
                memset(&stBox, 0, sizeof(stBox));
                stBox.left = 0;
                stBox.top = 0;
                stBox.front = 0;
                stBox.right = desc.Width - 1;
                stBox.bottom = desc.Height - 1;
                stBox.back = 1;
                pContext->CopySubresourceRegion(pTexDest, 0, 0, 0, 0, pTex11Shared, 0, &stBox);
#ifdef TEST_CLEARRECT
                if (pTargetView.Get())
                {
                    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
                    pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (LPVOID*)context1.GetAddressOf());
                    if (context1.Get())
                    {
                        D3D11_RECT rect = { 100, 100, 300, 300 };
                        FLOAT color[4] = { 0 };
                        context1->ClearView(pTargetView.Get(), color, &rect, 1);
                    }
                }
#endif
            }
            else
            {
                Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> pInputView = nullptr;
                Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> pOutputView = nullptr;
                D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = { 0, D3D11_VPIV_DIMENSION_TEXTURE2D, { 0, 0 } };
                hr = m_pVideoDevice->CreateVideoProcessorInputView(pTex11Shared, m_pVideoProcessorEnumerator.Get(), &inputViewDesc, &pInputView);
                if (!SUCCEEDED(hr) || !pInputView)
                {
                    DBGMSG(dbgERROR, L"%s - CreateVideoProcessorInputView failed with error - %d\n", WFUNCTION, hr);
                    return E_FAIL;
                }
                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = { D3D11_VPOV_DIMENSION_TEXTURE2D };

                hr = m_pVideoDevice->CreateVideoProcessorOutputView(pTexDest, m_pVideoProcessorEnumerator.Get(), &outputViewDesc, &pOutputView);
                if (!SUCCEEDED(hr) || !pOutputView)
                {
                    DBGMSG(dbgERROR, L"%s - CreateVideoProcessorOutputView failed with error - %d\n", WFUNCTION, hr);
                    return E_FAIL;
                }

                D3D11_VIDEO_PROCESSOR_STREAM stream = { TRUE, 0, 0, 0, 0, NULL, pInputView.Get(), NULL };
                hr = m_pVideoContext->VideoProcessorBlt(m_pVideoProcessor.Get(), pOutputView.Get(), 0, 1, &stream);
                if (!SUCCEEDED(hr))
                {
                    DBGMSG(dbgERROR, L"%s - VideoProcessorBlt failed with error - %d\n", WFUNCTION, hr);
                }
            }
        }
        break;

        case DXGI_FORMAT_NV12:
        {
            Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> pInputView = nullptr;
            Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> pOutputView = nullptr;
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = { 0, D3D11_VPIV_DIMENSION_TEXTURE2D, { 0, 0 } };

            HRESULT hr = m_pVideoDevice->CreateVideoProcessorInputView(pTex11Shared, m_pVideoProcessorEnumerator.Get(), &inputViewDesc, &pInputView);
            if (!SUCCEEDED(hr) || !pInputView)
            {
                DBGMSG(dbgERROR, L"%s - CreateVideoProcessorInputView failed with error - %d\n", WFUNCTION, hr);
                return E_FAIL;
            }
            D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = { D3D11_VPOV_DIMENSION_TEXTURE2D };

            hr = m_pVideoDevice->CreateVideoProcessorOutputView(pTexDest, m_pVideoProcessorEnumerator.Get(), &outputViewDesc, &pOutputView);
            if (!SUCCEEDED(hr) || !pOutputView)
            {
                DBGMSG(dbgERROR, L"%s - CreateVideoProcessorOutputView failed with error - %d\n", WFUNCTION, hr);
                return E_FAIL;
            }

            D3D11_VIDEO_PROCESSOR_STREAM stream = { TRUE, 0, 0, 0, 0, NULL, pInputView.Get() , NULL };
            hr = m_pVideoContext->VideoProcessorBlt(m_pVideoProcessor.Get(), pOutputView.Get(), 0, 1, &stream);
            if (!SUCCEEDED(hr))
            {
                DBGMSG(dbgERROR, L"%s - VideoProcessorBlt failed with error - %d\n", WFUNCTION, hr);
            }

#ifdef TEST_CLEARRECT
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pYTargetView, pUVTargetView;
            D3D11_RENDER_TARGET_VIEW_DESC rtv;
            rtv.Format = DXGI_FORMAT_R8_UINT;
            rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtv.Texture2D.MipSlice  = 0;

            hr = m_pDevice->CreateRenderTargetView(pTexDest, &rtv, &pYTargetView);
            if (!SUCCEEDED(hr) || !pYTargetView.Get())
            {
                DBGMSG(dbgERROR, L"%s - CreateRenderTargetView failed with %x error", WFUNCTION, hr);
            }

            if (pYTargetView)
            {
                D3D11_RENDER_TARGET_VIEW_DESC rtv;
                rtv.Format = DXGI_FORMAT_R8G8_UINT;
                rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv.Texture2D.MipSlice  = 0;
                hr = m_pDevice->CreateRenderTargetView(pTexDest, &rtv, &pUVTargetView);
                if (!SUCCEEDED(hr) || !pYTargetView.Get())
                {
                    DBGMSG(dbgERROR, L"%s - CreateRenderTargetView failed with %x error", WFUNCTION, hr);
                }
            }

            if (pYTargetView.Get() && pUVTargetView.Get())
            {
                Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
                pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (LPVOID*)context1.GetAddressOf());
                if (context1.Get())
                {
                    D3D11_RECT rect = { 200, 200, 250, 250 };

                    rect.left = m_frameCnt + 50;
                    rect.right = rect.left + 50;

                    if (rect.left > m_encodeParams.width || rect.right > m_encodeParams.width)
                    {
                        rect.left = 0;
                        rect.right = rect.left + 50;
                    }

                    FLOAT color[4] = { 0x10 };
                    context1->ClearView(pYTargetView.Get(), color, &rect, 1);

                    FLOAT colorUV[4] = { 0x80 };
                    context1->ClearView(pUVTargetView.Get(), colorUV, &rect, 1);
                }
            }
#endif
        }
        break;
    }
    pContext->Flush();
    return hr;
} //CopySharedTexture

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::Encode
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::Encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    HRESULT hr = S_OK;
    ID3D11DeviceContext *pImmediateContext = nullptr;

    if (!pEncodeInfo || !pBuffer)
    {
        DBGMSG(dbgERROR, L"%s - Null pointer to either pEncodeInfo - %x or pBuffer - %x passed", WFUNCTION, pEncodeInfo, pBuffer);
        return E_INVALIDARG;
    }

    if (!m_bNvEncInterfaceInitialized || !m_pEncodeD3D11.get())
    {
        DBGMSG(dbgERROR, L"%s - NvEnc object either not created or initialized", WFUNCTION);
        return E_NOT_SET;
    }

    std::vector<std::vector<uint8_t>> vPacket;
    uint32_t bitStreamSize = 0;
    NV_ENC_PIC_PARAMS picParams;
    NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };
    ID3D11Texture2D* pTex11Shared = nullptr;
    bool bSyncAcquired = false;
    IDXGIKeyedMutex* pDXGIKeyedMutex = nullptr;

    //Keeping a cleanup routine for the function
    ScopeGuards cleanup;
    auto cleanupFunc = [&, this]()
    { 
        if (pImmediateContext)
        {
            pImmediateContext->Release();
            pImmediateContext = nullptr;
        }
        if (pDXGIKeyedMutex)
        {
            // release the mutex.
            if (bSyncAcquired)
            {
                DBGMSG(dbgPROFILE, L"%s ReleaseSync on %x texture with %x key", WFUNCTION, pTex11Shared, DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY);
                pDXGIKeyedMutex->ReleaseSync(DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY);
            }
            pDXGIKeyedMutex->Release();
            pDXGIKeyedMutex = nullptr;
        }
    };

    cleanup += cleanupFunc;

    try
    {
        switch (pEncodeInfo->bufferInfo.bufferType)
        {
        case kBufferType_Vid:
            if (pEncodeInfo->bufferInfo.DxBuffer.bufferHandle)
            {
                //First check if it is an existing handle
                SurfaceHandleMap::const_iterator result = m_surfaceHandleMap.find((uint64_t)pEncodeInfo->bufferInfo.DxBuffer.bufferHandle);
                if (result == m_surfaceHandleMap.end())
                {
                    DBGMSG(dbgINFO, L"%s No mapping for %x handle, Trying to Map", WFUNCTION, (uint64_t)pEncodeInfo->bufferInfo.DxBuffer.bufferHandle);
                    hr = m_pDevice->OpenSharedResource(pEncodeInfo->bufferInfo.DxBuffer.bufferHandle, __uuidof(ID3D11Texture2D),
                        (LPVOID*)&pTex11Shared);
                    if (FAILED(hr) || !pTex11Shared)
                    {
                        DBGMSG(dbgERROR, L"%s - OpenSharedResource for %x handle failed with %x error", WFUNCTION, pEncodeInfo->bufferInfo.DxBuffer.bufferHandle, hr);
                        return E_FAIL;
                    }

                    UINT32 evictionPriority = pTex11Shared->GetEvictionPriority();
                    if (evictionPriority != DXGI_RESOURCE_PRIORITY_MAXIMUM)
                    {
                        DBGMSG(dbgPROFILE, L"%s - Setting Eviction Prioity to maximum", WFUNCTION);
                        pTex11Shared->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);
                    }
                    DBGMSG(dbgINFO, L"%s - %x resource with %x handle being cached", WFUNCTION, pTex11Shared, pEncodeInfo->bufferInfo.DxBuffer.bufferHandle);
                    insertMap((uint64_t)pEncodeInfo->bufferInfo.DxBuffer.bufferHandle, (uint64_t)pTex11Shared);
                }
                else
                {
                    pTex11Shared = reinterpret_cast<ID3D11Texture2D*>(result->second);
                }

                if (!pTex11Shared)
                {
                    DBGMSG(dbgERROR, L"%s No mapping for %x handle", WFUNCTION, (uint64_t)pEncodeInfo->bufferInfo.DxBuffer.bufferHandle);
                    return E_FAIL;
                }
                m_pDevice->GetImmediateContext(&pImmediateContext);
                if (!pImmediateContext)
                {
                    DBGMSG(dbgERROR, L"%s - GetImmediateContext failed with null context", WFUNCTION);
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    return hr ? hr : E_FAIL;
                }
                //clear the packet
                vPacket.clear();

                if (pTex11Shared)
                {
                    hr = pTex11Shared->QueryInterface(__uuidof(IDXGIKeyedMutex),
                        (LPVOID*)&pDXGIKeyedMutex);
                    if (FAILED(hr) || (pDXGIKeyedMutex == NULL))
                    {
                        DBGMSG(dbgERROR, L"%s - QueryInterface on IDXGIKeyedMutex return null, returning error", WFUNCTION);
                        return hr;
                    }
                    if (pDXGIKeyedMutex)
                    {
                        tictoc timer1;
                        DWORD result = pDXGIKeyedMutex->AcquireSync(DXSURFACE_CREATORRELEASE_USERACQUIRE_KEY, 1);
                        if (result == WAIT_OBJECT_0)
                        {
                            DBGMSG(dbgPROFILE, L"%s - AcquireSync for %x handle succeeded", WFUNCTION, pEncodeInfo->bufferInfo.DxBuffer.bufferHandle);
                            bSyncAcquired = true;
                        }
                        else
                        {
                            DBGMSG(dbgERROR, L"%s - AcquireSync failed for %x texture with %x handle with error - %d\n", WFUNCTION, pTex11Shared, pEncodeInfo->bufferInfo.DxBuffer.bufferHandle, result);
                            //return S_OK; //returning S_OK and dropping the frame
                        }
                        double elapsedTime = timer1.getelapsed();
                        DBGMSG(dbgPROFILE, L"%s - time-taken for AcquireSync at frame count - %x:  %lf mecs", WFUNCTION, m_frameCnt, elapsedTime);
                    }

                    const NvEncInputFrame* encoderInputFrame = m_pEncodeD3D11->GetNextInputFrame();
                    if (!encoderInputFrame)
                    {
                        DBGMSG(dbgINFO, L"%s - GetNextInputFrame return null frame, queue probably full", WFUNCTION);
                        return S_OK; //returning S_OK so that the encoder in the pipeline doesnt stop processing, essentially frame dropped
                    }
                    ID3D11Texture2D *pTexNvEncQueue = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
                    assert(pTexNvEncQueue);

                    hr = CopySharedTexture(pImmediateContext, pTexNvEncQueue, pTex11Shared, m_d3dFormat);
                    if (!SUCCEEDED(hr))
                    {
                        DBGMSG(dbgERROR, L"%s - CopySharedTexture failed with error - %d\n", WFUNCTION, hr);
                    }
                    if (pDXGIKeyedMutex)
                    {
                        // release the mutex.
                        DBGMSG(dbgPROFILE, L"%s ReleaseSync on %x texture with %x key", WFUNCTION, pTex11Shared, DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY);
                        pDXGIKeyedMutex->ReleaseSync(DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY);
                        bSyncAcquired = false;
                        pDXGIKeyedMutex->Release();
                        pDXGIKeyedMutex = nullptr;
                    }
                    memset(&picParams, 0, sizeof(picParams));
                    picParams.version = NV_ENC_PIC_PARAMS_VER;
                    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
                    picParams.inputTimeStamp = pEncodeInfo->pts;
                    bool bRet = m_pEncodeD3D11.get()->EncodeFrame(vPacket, &picParams, &lockBitstreamData);
                    if (!bRet)
                    {
                        DBGMSG(dbgINFO, L"%s - Frame dropped, probably nvEnc queue full", WFUNCTION);
                    }
#ifdef DUMP_TEXTURE
                    m_pContext->CopySubresourceRegion(m_pTexStagingMem.Get(), 0, 0, 0, 0, pTex11Shared, 0, pBox);
                    D3D11_MAPPED_SUBRESOURCE map;

                    hr = m_pContext->Map(m_pTexStagingMem.Get(), D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &map);
                    if (SUCCEEDED(hr))
                    {
                        DWORD val = *(uint32_t*)(map.pData);
                        DBGMSG(dbgLogERROR,__FUNCTION__"\n Value - %x\n", val);
                    }
#endif
                }
            }
            break;
        case kBufferType_Sys:
            if (pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer)
            {
                const NvEncInputFrame* encoderInputFrame = m_pEncodeD3D11->GetNextInputFrame();
                D3D11_MAPPED_SUBRESOURCE map;
                hr = m_pContext->Map(m_pTexStagingMem.Get(), D3D11CalcSubresource(0, 0, 1), D3D11_MAP_WRITE, 0, &map);
                if (SUCCEEDED(hr))
                {
                    switch (m_encodeParams.bufferFormat)
                    {
                    case kBufferFormat_ARGB:
                    {
                        for (unsigned int y = 0; y < m_encodeParams.height; y++)
                        {
                            memcpy((uint8_t *)map.pData + y * map.RowPitch, (uint8_t *)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer + y * pEncodeInfo->bufferInfo.SysBuffer.lineWidth * 4), m_encodeParams.width * 4);
                        }
                    }
                    break;
                    case kBufferFormat_NV12:
                    {
                        for (unsigned int y = 0; y < m_encodeParams.height; y++)
                        {
                            memcpy((uint8_t *)map.pData + y * map.RowPitch, (uint8_t *)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer + y * pEncodeInfo->bufferInfo.SysBuffer.lineWidth), m_encodeParams.width * 1);
                        }

                        uint8_t* pUVDest = reinterpret_cast<uint8_t*>((uint64_t)(map.pData) + map.RowPitch * m_encodeParams.height);
                        uint8_t* pSrc = reinterpret_cast<uint8_t*>((uint64_t)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer) + m_encodeParams.height * pEncodeInfo->bufferInfo.SysBuffer.lineWidth * 1);
                        for (unsigned int y = 0; y < m_encodeParams.height / 2; y++)
                        {
                            memcpy((uint8_t *)pUVDest, pSrc, m_encodeParams.width);
                            pUVDest += map.RowPitch;
                            pSrc += pEncodeInfo->bufferInfo.SysBuffer.lineWidth;
                        }
                    }
                    break;
                    default:
                        hr = E_INVALIDARG;
                        break;
                    }
                    m_pContext->Unmap(m_pTexStagingMem.Get(), D3D11CalcSubresource(0, 0, 1));
                    if (SUCCEEDED(hr))
                    {
                        ID3D11Texture2D *pTexBgra = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
                        m_pContext->CopyResource(pTexBgra, m_pTexStagingMem.Get());
                        m_pContext->Flush();

                        memset(&picParams, 0, sizeof(picParams));
                        picParams.version = NV_ENC_PIC_PARAMS_VER;
                        picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
                        picParams.inputTimeStamp = pEncodeInfo->pts;
                        m_pEncodeD3D11.get()->EncodeFrame(vPacket, &picParams, &lockBitstreamData);
                    }
                }
            }
            break;
        }
    }
    catch (std::exception& e)
    {
        DBGMSG(dbgERROR, L"%s: NvEncoderD3D11 EncodeFrame possibly threw exception : %s ", WFUNCTION, e.what());
#ifdef DEBUG
        DebugBreak();
#endif
        hr = E_FAIL;
    }
    catch (...)
    {
        DBGMSG(dbgERROR, L"%s: NvEncoderD3D11 EncodeFrame threw unhandled exception", WFUNCTION);
#ifdef DEBUG
        DebugBreak();
#endif
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        ++m_frameCnt;
        for (std::vector<uint8_t> &packet : vPacket)
        {
            //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
            bitStreamSize += static_cast<uint32_t>(packet.size());
        }

        uint8_t* pDest = nullptr;
        nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
        if (bitStreamSize && pBitStreamObj)
        {
            pDest = pBitStreamObj->allocBuffer(bitStreamSize);
        }

        if (pDest)
        {
            uint32_t size = 0;
            for (std::vector<uint8_t> &packet : vPacket)
            {
                memcpy(pDest, packet.data(), packet.size());
                pDest += packet.size();
                //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
                size += static_cast<uint32_t>(packet.size());
                if (size > bitStreamSize)
                {
                    DBGMSG(dbgERROR, L"%s - encoded Bitstream size - %x exceeding allocated size - %x, possibly clippiong of data", WFUNCTION, size, bitStreamSize);
                    assert(0);
                    break;
                }
            }
            pBitStreamObj->setBitstreamParams(&lockBitstreamData);
        }
    }
    return hr;
} //Encode

//-------------------------------------------------------------------
// nvEncBroadcastObj::Finalize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::Finalize(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    HRESULT hr = S_OK;
    std::vector<std::vector<uint8_t>> vPacket;
    uint32_t bitStreamSize = 0;
    NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };

    if (!m_bNvEncInterfaceInitialized || !m_pEncodeD3D11.get())
    {
        return E_NOT_SET;
    }

    m_pEncodeD3D11.get()->EndEncode(vPacket, &lockBitstreamData);
    for (std::vector<uint8_t> &packet : vPacket)
    {
        //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
        bitStreamSize += static_cast<uint32_t>(packet.size());
    }

    uint8_t* pDest = nullptr;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (pBitStreamObj && bitStreamSize)
    {
        pDest = pBitStreamObj->allocBuffer(bitStreamSize);
    }

    if (pDest)
    {
        uint32_t size = 0;
        for (std::vector<uint8_t> &packet : vPacket)
        {
            memcpy(pDest, packet.data(), packet.size());
            pDest += packet.size();
            //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
            size += static_cast<uint32_t>(packet.size());
            if (size > bitStreamSize)
            {
                assert(0);
                hr = ERROR_INCORRECT_SIZE;
                break;
            }
        }
        pBitStreamObj->setBitstreamParams(&lockBitstreamData);
    }
    return hr;
} //Finalize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::GetSequenceParams
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::GetSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    if (!pBuffer)
    {
        return E_INVALIDARG;
    }

    if (!m_bNvEncInterfaceInitialized || !m_pEncodeD3D11.get())
    {
        return E_NOT_SET;
    }

    std::vector<uint8_t> vPacket;
    uint32_t bitStreamSize = 0;

    vPacket.clear();
    m_pEncodeD3D11.get()->GetSequenceParams(vPacket);
    //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
    bitStreamSize += static_cast<uint32_t>(vPacket.size());

    uint8_t* pDest = nullptr;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (bitStreamSize && pBitStreamObj)
    {
        pDest = pBitStreamObj->allocBuffer(bitStreamSize);
    }

    if (pDest)
    {
        memcpy(pDest, vPacket.data(), vPacket.size());
    }
    return S_OK;
} //GetSequenceParams

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::CreateInstance
//-------------------------------------------------------------------
nvEncBroadcastInterfaceImplD3D11* nvEncBroadcastInterfaceImplD3D11::CreateInstance()
{
    nvEncBroadcastInterfaceImplD3D11* pEncodeObj = nullptr;
    pEncodeObj = new nvEncBroadcastInterfaceImplD3D11();
    if (!pEncodeObj)
    {
        return NULL;
    }
    return pEncodeObj;
} //CreateInstance

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::insertMap
//-------------------------------------------------------------------
bool nvEncBroadcastInterfaceImplD3D11::insertMap(const uint64_t handle, uint64_t ptr)
{
    SurfaceHandlePair obj;
    obj.first = handle;
    obj.second = ptr;
    auto ret = m_surfaceHandleMap.insert(obj);
    if (!ret.second) {
        DBGMSG(dbgERROR, L"%s:Failed to insert %x object for %x ptr", __FUNCTION__, handle, ptr);
        return false;
    }
    return true;
} // insertMap

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::deleteMap
//-------------------------------------------------------------------
bool nvEncBroadcastInterfaceImplD3D11::deleteMap(uint64_t handle)
{
    m_surfaceHandleMap.erase(handle);
    return true;
} // deleteMap

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::initialize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    return Initialize(pEncodeParams, pEncodeSettingsParams);
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::encode
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return Encode(pEncodeInfo, pBuffer);
} //encode

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::finalize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::finalize(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return Finalize(pBuffer);
} //finalize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::uploadToTexture
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::uploadToTexture()
{
    return S_OK;
    //return ploadToTexture(pBuffer);
} //uploadToTexture

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::nvEncBroadcastInterfaceImplD3D11::release()
{
    return Release();
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D11::release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D11::getSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return GetSequenceParams(pBuffer);
}  //getSequenceParams