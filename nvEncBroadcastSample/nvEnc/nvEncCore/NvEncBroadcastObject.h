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
#include <list>
#include "nvEncBroadcastEncodeApi.h"
#include "nvEncBroadcastBitstreamBuffer.h"
#include "nvEncBroadcastInterface.h"

#ifndef FAILED_NVENC_BROADCASTAPI
#define FAILED_NVENC_BROADCASTAPI(retCode) ((retCode) != API_SUCCESS)
#endif

#ifndef SUCCEEDED_NVENC_BROADCASTAPI
#define SUCCEEDED_NVENC_BROADCASTAPI(retCode) ((retCode) == API_SUCCESS)
#endif


class nvEncBroadcastObj : public nvEncBroadcastApi::INVENC_EncodeApiObj
{

protected:
    bool                         m_bNvEncInitialized;
    uint32_t                     m_processId;
    nvEncBroadcastApi::NVENC_EncodeCreateParams m_createParams;
    nvEncBroadcastApi::NVENC_EncodeInitParams   m_encodeParams;
    uint32_t                     m_RefCount;
    std::unique_ptr<INvEncBroadcastInterface>  m_pEncodeImpl;
    std::list<uint64_t>          m_ptsList;
    D3DFORMAT                    m_d3dFormat;
    uint64_t                     m_frameCnt;
    uint64_t                     m_PrevTimeStamp;
    uint64_t                     m_timeStampDiff;
    static uint32_t              m_InstanceCount;

    virtual ~nvEncBroadcastObj();
    nvEncBroadcastObj(nvEncBroadcastApi::NVENC_EncodeCreateParams* pCreateParams);
    virtual HRESULT Initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams);
    virtual HRESULT Encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);
    virtual HRESULT GetSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer);

public:
    static nvEncBroadcastObj*  CreateInstance(nvEncBroadcastApi::NVENC_EncodeCreateParams* pCreateParams);

    // INVENC_EncodeApiObj methods
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams) override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func createBitstreamBuffer(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer** ppBuffer) override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func getSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* ppBuffer) override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) override;
    virtual bool NVENCBROADCAST_Func isInitalized() const override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func releaseObject() override;

};
