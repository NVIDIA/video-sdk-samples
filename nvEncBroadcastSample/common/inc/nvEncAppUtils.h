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

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdint.h>
#include <string>
#include <d3d9.h>
#include <d3d11.h>
#include "nvEncBroadcastEncodeApi.h"
#include <memory>

#define NVENC_SAMPLEDLL          "nvEncBroadcast.dll"          

typedef struct _AppEncodeParams {
    wchar_t  inputFile[MAX_LENGTH];
    wchar_t  outputFile[MAX_LENGTH];
    uint32_t width;
    uint32_t height;
    uint32_t numFrames;
}AppEncodeParams;

const D3DFORMAT D3DFMT_NV12 = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');

enum class DxContextType
{
    eDx9Type = 0,
    eDx11Type = 1,
    eDx12Type = 2,
    eDxTypeMax,
};

class INvEncDxInterop
{
public:
    INvEncDxInterop()
        : m_dxContextType(DxContextType::eDxTypeMax)
    {
    }
    virtual ~INvEncDxInterop() = 0 {
    }
    virtual bool  Initialize(const nvEncBroadcastApi::NVENC_EncodeInitParams* pVideoInfo) = 0;
    virtual bool  SetFrameParams(nvEncBroadcastApi::NVENC_EncodeInfo& encodeInfo, const nvEncBroadcastApi::NVENC_EncodeInitParams& initParams, const uint8_t* pSrc) = 0;
    virtual DxContextType  GetDxInteropType() const = 0;
protected:
    DxContextType m_dxContextType;
};

namespace CNvEncDxInteropFactory
{
    std::unique_ptr<INvEncDxInterop> CreateDxInterop(DxContextType type);
}