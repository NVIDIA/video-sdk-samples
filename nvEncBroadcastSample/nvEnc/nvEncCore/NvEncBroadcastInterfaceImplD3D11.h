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

#include <wrl.h>
#include "nvEncBroadcastEncodeApi.h"
#include "nvEncBroadcastInterface.h"
#include "NvEncoder/nvEncodeAPI.h"
#include "NvEncoder/NvEncoderD3D9.h"
#include "NvEncoder/NvEncoderD3D11.h"
#include <d3d11.h>
#include <d3d11_1.h>
#include <map>

#define  MAX_ENCODE_BUFFERS                20                            //Considering Local ahead which might go to 10 + Number of frames + output delay
#define  MAX_OUTPUT_DELAY_BUFFERS          3
#define  EXTRA_BUFFERS_DELAY_NVENC         6

template <typename T1, typename T2>
using nvEncD3D11SurfaceHandlePair = std::pair<T1, T2>;

template <typename T1, typename T2>
using nvEncD3D11SurfaceHandleMap = std::map<T1, T2>;

using SurfaceHandleMap = nvEncD3D11SurfaceHandleMap<uint64_t, uint64_t>;
using SurfaceHandlePair = nvEncD3D11SurfaceHandlePair<uint64_t, uint64_t>;

class nvEncBroadcastInterfaceImplD3D11 : public INvEncBroadcastInterface
{
protected:
    bool                         m_bNvEncInterfaceInitialized;
    uint32_t                     m_processId;
    nvEncBroadcastInterfaceCreateParams m_createParams;
    nvEncBroadcastApi::NVENC_EncodeInitParams   m_encodeParams;
    uint32_t                     m_RefCount;
    uint32_t                     m_ulEncodeBuffers;
    std::unique_ptr<NvEncoderD3D11, nvEncCustomDeleter>  m_pEncodeD3D11;
    Microsoft::WRL::ComPtr<ID3D11Device> m_pD3DDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pD3DDeviceContext;
    Microsoft::WRL::ComPtr<ID3D11Device1> m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_pContext;
    Microsoft::WRL::ComPtr<IDXGIFactory1> m_pFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter> m_pAdapter;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> m_pVideoDevice;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> m_pVideoContext;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> m_pVideoProcessor;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> m_pVideoProcessorEnumerator;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pTexStagingMem;
    std::unique_ptr<ID3D11Texture2D>  m_pInputTexture[MAX_ENCODE_BUFFERS];
    DXGI_FORMAT                   m_d3dFormat;
    SurfaceHandleMap              m_surfaceHandleMap;
    int64_t                       m_frameCnt;
    bool                          m_bConvertToNV12;

    virtual ~nvEncBroadcastInterfaceImplD3D11();
    nvEncBroadcastInterfaceImplD3D11();

    HRESULT Initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams);
    HRESULT InitializeDxVA();
    HRESULT Release();
    HRESULT Encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    HRESULT Finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    HRESULT GetSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    HRESULT AllocateInputBuffers(uint32_t numInputBuffers, NV_ENC_BUFFER_FORMAT nvEncFormat);
    void    ReleaseResources();
    HRESULT CopySharedTexture(ID3D11DeviceContext* pContext, ID3D11Texture2D* pTexDest, ID3D11Texture2D* pTex11Shared, DXGI_FORMAT d3dFormat);
    bool insertMap(uint64_t handle, uint64_t ptr);
    bool deleteMap(uint64_t handle);

public:
    static nvEncBroadcastInterfaceImplD3D11*  CreateInstance();

    // INvEncBroadcastInterface methods
    HRESULT initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams) override;
    HRESULT release() override;
    HRESULT uploadToTexture() override;
    HRESULT encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    HRESULT finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    HRESULT getSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
};
