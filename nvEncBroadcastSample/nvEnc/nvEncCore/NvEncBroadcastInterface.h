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

#include "nvEncBroadcastEncodeApi.h"
#include "NvEncoder/nvEncodeAPI.h"
#include "nvEncoder/NvEncoder.h"
#include <d3d9.h>
#include <memory>

//forward declarations
//todo: to convert initializeEncodeConfig to a template function such that it doesnt have a dependency on lower level classes
class NvEncoder;

const D3DFORMAT D3DFMT_NV12 = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');

#define   MAX_ENCODE_BFRAMES                     3
#define   MAX_BITRATE_FOR_RATECQ                 40000
#define   DEFAULT_OUTPUT_FPS_NUM                 30000   //Defaulting to 30 fps
#define   DEFAULT_OUTPUT_FPS_DEN                 1000

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceCreateParams struct
//-------------------------------------------------------------------
typedef struct _nvEncBroadcastInterfaceCreateParams
{
    uint32_t                           size;                    // IN parameter, size of the struct
    uint32_t                           version;                 // IN parameter, Version 
} nvEncBroadcastInterfaceCreateParams;

//-------------------------------------------------------------------
// eNvEncType enum
//-------------------------------------------------------------------
enum class eNvEncType {
    eNvEncoderType_DX9 = 0x00000001,
    eNvEncoderType_DX10 = 0x00000002,
    eNvEncoderType_DX11 = 0x00000004,
    eNvEncoderType_DX12 = 0x00000008,
    eNvEncoderType_Max,
};

//-------------------------------------------------------------------
// nvEncCustomDeleter - Custom deleter for nvEnc Objects
//-------------------------------------------------------------------
struct nvEncCustomDeleter
{
  void operator()(NvEncoder* pEncoder)
  {
    if (pEncoder)
    {
        pEncoder->DestroyEncoder();
        delete pEncoder;
        pEncoder = nullptr;
    }
  }
};

//-------------------------------------------------------------------
// INvEncBroadcastInterface interface defintion
// abstract class
//-------------------------------------------------------------------
class NVENCBROADCAST_NOVTABLE INvEncBroadcastInterface
{
public:
    //create or instantiate the feature
    virtual HRESULT initialize(nvEncBroadcastApi::NVENC_EncodeInitParams* pEncodeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams) = 0;
    virtual HRESULT release() = 0;
    virtual HRESULT uploadToTexture() = 0;
    virtual HRESULT encode(nvEncBroadcastApi::NVENC_EncodeInfo* pEncodeInfo, nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) = 0;
    virtual HRESULT finalize(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) = 0;
    virtual HRESULT getSequenceParams(nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer* pBuffer) = 0;
    virtual ~INvEncBroadcastInterface() = 0 {
    }
protected:
    INvEncBroadcastInterface() {
    }
    HRESULT initializeEncodeConfig(NvEncoder* pEncodeObj, NV_ENC_INITIALIZE_PARAMS* pInitializeParams, nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams);
    bool validateEncodingParams(nvEncBroadcastApi::NVENC_EncodeSettingsParams* pEncodeSettingsParams);
};

namespace CNvEncBroadcastFactory
{
    std::unique_ptr<INvEncBroadcastInterface> CreateNvEncInstance(eNvEncType type);
}
