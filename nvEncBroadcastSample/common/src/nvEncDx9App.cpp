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

#include "nvEncDx9App.h"
#include <memory>
#include <cassert>

using namespace Microsoft::WRL;
using namespace nvEncBroadcastApi;
//-------------------------------------------------------------------
// CNvEncDx9Interop::CNvEncDx9Interop
//-------------------------------------------------------------------
CNvEncDx9Interop::CNvEncDx9Interop()
    : m_pD3D9(nullptr)
    , m_pDevice(nullptr)
    , m_pTex(nullptr)
{
    m_dxContextType = DxContextType::eDx9Type;
} //constructor

//-------------------------------------------------------------------
// CNvEncDx9Interop::~CNvEncDx9Interop
//-------------------------------------------------------------------
CNvEncDx9Interop::~CNvEncDx9Interop()
{

} //destructor

//-------------------------------------------------------------------
// CNvEncDx9Interop::Initialize
//-------------------------------------------------------------------
bool CNvEncDx9Interop::Initialize(const NVENC_EncodeInitParams* pVideoInfo)
{
    D3DFORMAT d3dFormat = D3DFMT_A8R8G8B8;

    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &(m_pD3D9));
    if (!SUCCEEDED(hr) || !m_pD3D9)
    {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.BackBufferWidth = pVideoInfo->width;
    d3dpp.BackBufferHeight = pVideoInfo->height;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.Windowed = TRUE;
    d3dpp.hDeviceWindow = nullptr;

    hr = m_pD3D9->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &m_pDevice);
    if (!SUCCEEDED(hr) || !m_pDevice)
    {
        return false;
    }

    D3DADAPTER_IDENTIFIER9 id;
    m_pD3D9->GetAdapterIdentifier(0, 0, &id);
    if (SUCCEEDED(hr))
    {
        return true;
    }
    return false;
} //Initialize

//-------------------------------------------------------------------
// CNvEncDx9Interop::SetFrameParams
//-------------------------------------------------------------------
bool CNvEncDx9Interop::SetFrameParams(nvEncBroadcastApi::NVENC_EncodeInfo& encodeInfo, const nvEncBroadcastApi::NVENC_EncodeInitParams& initParams, const uint8_t* pSrc)
{
    HRESULT hr = S_OK;
    if (!pSrc)
    {
        return false;
    }
    encodeInfo.bufferInfo.bufferType = kBufferType_Sys;
    encodeInfo.bufferInfo.bufferFormat = initParams.bufferFormat;
    encodeInfo.bufferInfo.SysBuffer.lineWidth = initParams.width;
    encodeInfo.bufferInfo.SysBuffer.pixelBuffer = reinterpret_cast<uint64_t>(pSrc);
    return SUCCEEDED(hr) ? true : false;
} //Encode

bool CNvEncDx9Interop::ReleaseNvEnc()
{
    return true;
} //ReleaseNvEnc