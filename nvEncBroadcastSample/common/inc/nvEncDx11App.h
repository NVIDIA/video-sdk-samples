/*
* Copyright 2018 NVIDIA Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this
* software and associated documentation files (the "Software"),  to deal in the Software
* without restriction, including without limitation the rights to use, copy, modify,
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in all copies
* or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
* OR OTHER DEALINGS IN THE SOFTWARE.
*
*/

#pragma once

#include "nvEncBroadcastEncodeApi.h"
#include "nvEncAppUtils.h"
#include "wrl.h"


class CNvEncDx11Interop: public INvEncDxInterop 
{
public:
    CNvEncDx11Interop();
    virtual ~CNvEncDx11Interop();
    bool Initialize(const nvEncBroadcastApi::NVENC_EncodeInitParams* pVideoInfo);
    bool SetFrameParams(nvEncBroadcastApi::NVENC_EncodeInfo& encodeInfo, const nvEncBroadcastApi::NVENC_EncodeInitParams& initParams, const uint8_t* pSrc);
    DxContextType  GetDxInteropType() const {
        return m_dxContextType;
    }
protected:
    bool ReleaseNvEnc();
    Microsoft::WRL::ComPtr<ID3D11Device>        m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext;
    Microsoft::WRL::ComPtr<IDXGIFactory1>       m_pFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter>        m_pAdapter;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_pTexSysMem;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_pTexSharedMem;
    HANDLE                                      m_sharedHandle;
};