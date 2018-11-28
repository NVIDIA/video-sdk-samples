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
#include "nvEncBroadcastEncodeApi.h"
#include "allocator.h"
#include "nvEncoder\nvEncodeAPI.h"

template <typename T, class Alloc = Allocator<T>>
class nvEncBroadcastBitstreamBufferObject : public nvEncBroadcastApi::INVENC_EncodeBitstreamBuffer
{
protected:
    bool                         m_bBufferInitialized;
    uint32_t                     m_RefCount;

    virtual ~nvEncBroadcastBitstreamBufferObject();
    nvEncBroadcastBitstreamBufferObject();
    virtual HRESULT Initialize();
    virtual HRESULT Release();
    virtual HRESULT Recycle();

    //time stamps
    int64_t                      m_ptsArray[nvEncBroadcastApi::eNVENC_TimeStampType::NVENC_TimeStamp_Max];
    T*                           m_pData;
    uint32_t                     m_DataSize;
    Alloc                        allocatorObj;
    NV_ENC_LOCK_BITSTREAM        m_bitStreamData;
public:
    static nvEncBroadcastBitstreamBufferObject* CreateInstance();

    //internal helper functions implemented inline due to being a template function being called internally
    void setPts(nvEncBroadcastApi::eNVENC_TimeStampType tsType, uint64_t pts) {
        if (tsType >= nvEncBroadcastApi::eNVENC_TimeStampType::NVENC_TimeStamp_Max)
            return;
        m_ptsArray[tsType] = pts;
    } //setPts

    void setBitstreamParams(NV_ENC_LOCK_BITSTREAM* pBitStreamData) {
        if (pBitStreamData)
            memcpy(&m_bitStreamData, pBitStreamData, sizeof(m_bitStreamData));
    } //setBitstreamParams

    const NV_ENC_LOCK_BITSTREAM* getBitStreamParams() const { return &m_bitStreamData;  }

    virtual bool NVENCBROADCAST_Func isInitalized() const;

    uint8_t* allocBuffer(uint32_t size) {
        if (!size)
        {
            return nullptr;
        }

        if (m_pData)
        {
            Allocator<uint8_t>::deallocate(m_pData, m_DataSize, true);
            m_DataSize = 0;
        }

        m_pData = Allocator<uint8_t>::allocate(size);
        if (m_pData)
        {
            m_DataSize = size;
            return (uint8_t*)m_pData;
        }
        return nullptr;
    } //allocBuffer

    // INVENC_EncodeApiObj methods
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func initialize();
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func release() override;
    virtual nvEncBroadcastApi::eNVENC_RetCode NVENCBROADCAST_Func recycle() override;
    virtual uint64_t NVENCBROADCAST_Func getPts(nvEncBroadcastApi::eNVENC_TimeStampType tsType) const override {
        if (tsType >= nvEncBroadcastApi::eNVENC_TimeStampType::NVENC_TimeStamp_Max)
            return 0;
        return m_ptsArray[tsType];
    } //getPts

    virtual bool NVENCBROADCAST_Func isKeyFrame() const override;
    virtual uint8_t* NVENCBROADCAST_Func getBitStreamBuffer(uint32_t* size) const override;
};