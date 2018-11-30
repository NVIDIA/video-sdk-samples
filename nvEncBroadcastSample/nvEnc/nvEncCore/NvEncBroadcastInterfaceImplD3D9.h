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

#pragma once

#include <memory>
#include <wrl.h>
#include <stdint.h>
#include <d3d9.h>
#include <Dxva2api.h>
#include "nvEncBroadcastEncodeApi.h"
#include "nvEncBroadcastInterface.h"
#include "NvEncoder/nvEncodeAPI.h"
#include "NvEncoder/NvEncoderD3D9.h"


class nvEncBroadcastDirectXContext {
protected:
    LPDIRECT3D9EX m_pDirect3D9;
    LPDIRECT3DDEVICE9EX m_pDirect3DDevice9;
    IDirectXVideoAccelerationService *m_pDXVA;
    IDirectXVideoProcessorService *m_pDXVAProc;
    BOOL m_bIsCOMRuntimeInitialized;
    static nvEncBroadcastDirectXContext context;
    uint32_t  m_width;
    uint32_t  m_height;

public:
    nvEncBroadcastDirectXContext() :
         m_pDirect3D9(NULL)
        , m_pDirect3DDevice9(NULL)
        , m_pDXVA(NULL)
        , m_pDXVAProc(NULL)
        , m_bIsCOMRuntimeInitialized(false)
        , m_width(0)
        , m_height(0)
        {
        }

    ~nvEncBroadcastDirectXContext() {
        instance()->Release(); 
    }

    static IDirect3DDevice9Ex * getDevice() { return instance()->m_pDirect3DDevice9; }
    static IDirectXVideoAccelerationService * getDXVA() { return instance()->m_pDXVA; }
    static IDirectXVideoProcessorService * getDXVAProc() { return instance()->m_pDXVAProc; }
    static nvEncBroadcastDirectXContext * instance();
    bool Create(uint32_t width = 128, uint32_t height = 128);
    void Release();
};


class nvEncBroadcastInterfaceImplD3D9 : public INvEncBroadcastInterface
{
protected:
    bool                         m_bNvEncInterfaceInitialized;
    uint32_t                     m_processId;
    nvEncBroadcastInterfaceCreateParams m_createParams;
    nvEncBroadcastApi::NVENC_EncodeInitParams   m_encodeParams;
    uint32_t                     m_RefCount;
    Microsoft::WRL::ComPtr<IDirect3DSurface9>  m_pSurfaceBgra;
    std::unique_ptr<NvEncoderD3D9, nvEncCustomDeleter> m_pEncodeD3D9;
    D3DFORMAT                    m_d3dFormat;

    ~nvEncBroadcastInterfaceImplD3D9();
    nvEncBroadcastInterfaceImplD3D9();
    HRESULT Initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams);
    HRESULT Release();
    HRESULT Encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    HRESULT Finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    HRESULT GetSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);

public:
    static nvEncBroadcastInterfaceImplD3D9*  CreateInstance();

    // INvEncBroadcastInterface methods
    HRESULT initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams) override;
    HRESULT release() override;
    HRESULT uploadToTexture() override;
    HRESULT encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    HRESULT finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    HRESULT getSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
};