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

#include <vector>
#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include <d3d9.h>
#include <dxva2api.h>
#include "NvEncoder.h"


class NvEncoderD3D9 : public NvEncoder
{
public:
    NvEncoderD3D9(IDirect3DDevice9* pD3D9Device, uint32_t nWidth, uint32_t nHeight, NV_ENC_BUFFER_FORMAT eBufferFormat, 
        IDirectXVideoAccelerationService* pDXVAService = nullptr, uint32_t nExtraOutputDelay = 3, bool bMotionEstimationOnly = false);
    virtual ~NvEncoderD3D9();
private:

    /**
    *  @brief This function is used to allocate input buffers for encoding.
    *  This function is an override of virtual function NvEncoder::AllocateInputBuffers().
    *  This function creates IDirect3DSurface9* which is used to accept input data.
    *  To obtain handle to input buffers application must call NvEncoder::GetNextInputFrame()
    */
    virtual void AllocateInputBuffers(int32_t numInputBuffers) override;

    /**
    *  @brief This function is used to release the input buffers allocated for encoding.
    *  This function is an override of virtual function NvEncoder::ReleaseInputBuffers().
    */
    virtual void ReleaseInputBuffers() override;
private:
    /**
    *  @brief This is a private function to release IDirect3DSurface9 surfaces used for encoding.
    */
    void ReleaseD3D9Resources();
private:
    IDirect3DDevice9* m_pD3D9Device = nullptr;
    IDirectXVideoAccelerationService* m_pDXVAService = nullptr;
};
