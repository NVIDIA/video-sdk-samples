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

#include "nvEncBroadcastObject.h"
#include <cassert>
#include <vector>
#include <fstream>
#include "nvEncBroadcastUtils.h"
#include "NvEncBroadcastInterfaceImplD3D9.h"

using namespace nvEncBroadcastApi;

#ifdef TEST_FILE
#include <iostream>
std::ofstream     g_fpOut;
#endif

//static variables and globals
uint32_t nvEncBroadcastObj::m_InstanceCount = 0;

//-------------------------------------------------------------------
// nvEncBroadcastObj::nvEncBroadcastObj
//-------------------------------------------------------------------
nvEncBroadcastObj::nvEncBroadcastObj(NVENC_EncodeCreateParams* pCreateParams)
           : m_bNvEncInitialized(false)
           , m_processId(0xFFFFFFFF)
           , m_d3dFormat(D3DFMT_A8R8G8B8)
           , m_RefCount(0)
           , m_frameCnt(0)
           , m_PrevTimeStamp(INVALID_VALUE)
           , m_timeStampDiff(0)

{
    memcpy(&m_createParams, pCreateParams, sizeof(m_createParams));
    memset(&m_encodeParams, 0, sizeof(m_encodeParams));

    m_ptsList.clear();

    m_pEncodeImpl.release();
    ++m_RefCount;
    ++nvEncBroadcastObj::m_InstanceCount;
} // (Constructor)

//-------------------------------------------------------------------
// nvEncBroadcastObj::~nvEncBroadcastObj
//-------------------------------------------------------------------
nvEncBroadcastObj::~nvEncBroadcastObj()
{
    INvEncBroadcastInterface* pEncoderImpl = m_pEncodeImpl.release();
    if (pEncoderImpl)
    {
        pEncoderImpl->release();
        pEncoderImpl = nullptr;
    }

    m_ptsList.clear();
    nvEncBroadcastDirectXContext::instance()->Release();
}// Destructor

//-------------------------------------------------------------------
// nvEncBroadcastObj::Initialize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastObj::Initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    HRESULT hr = S_OK;

    if (!pEncodeParams)
    {
        return E_INVALIDARG;
    }

    eNvEncType type = eNvEncType::eNvEncoderType_DX11;
    //hard coding for now
    if (IsWin7())
    {
        type = eNvEncType::eNvEncoderType_DX9;
    }
    else
    {
        type = eNvEncType::eNvEncoderType_DX11;
    }

    m_pEncodeImpl = CNvEncBroadcastFactory::CreateNvEncInstance(type);
    if (!m_pEncodeImpl.get())
    {
        DBGMSG(dbgERROR, L"%s: failed to create encoder object", WFUNCTION);
        return E_FAIL;
    }

    hr = m_pEncodeImpl.get()->initialize(pEncodeParams, pEncodeSettingsParams);
    if (!SUCCEEDED(hr))
    {
        return hr;
    }

#ifdef TEST_FILE
    char fileName[32];
    memset(fileName, 0, sizeof(fileName));
    uint32_t nBuf = snprintf(fileName, 32 - 1, "test_%x.h264", nvEncBroadcastObj::m_InstanceCount);
    g_fpOut = std::ofstream(fileName, std::ios::out | std::ios::binary);
#endif

    m_bNvEncInitialized = true;
    return hr;
} //Initialize

//-------------------------------------------------------------------
// nvEncBroadcastObj::Encode
//-------------------------------------------------------------------
HRESULT nvEncBroadcastObj::Encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    HRESULT hr = S_OK;
    if (!pEncodeInfo || !pBuffer)
    {
        return E_INVALIDARG;
    }

    DBGMSG(dbgPROFILE, L"%s - Recieved Dts value - %ld", WFUNCTION, pEncodeInfo->pts);
    m_ptsList.push_front(pEncodeInfo->pts);

    if (m_PrevTimeStamp == INVALID_VALUE)
    {
        m_PrevTimeStamp = pEncodeInfo->pts;
    }
    else if (!m_timeStampDiff)
    {
        m_timeStampDiff = pEncodeInfo->pts - m_PrevTimeStamp;
    }

    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (pBitStreamObj)
    {
        hr = m_pEncodeImpl.get()->encode(pEncodeInfo, pBuffer);
        if (!SUCCEEDED(hr))
        {
            DBGMSG(dbgERROR, L"%s - encode failed with %x error, encoded %x frames - %ld", WFUNCTION, hr, m_frameCnt);
            return hr;
        }

        uint32_t size;
        uint8_t* pBitStream = pBitStreamObj->getBitStreamBuffer(&size);
        if (pBitStream && size)
        {
            ++m_frameCnt;
#ifdef TEST_FILE
            if (g_fpOut.is_open())
            {
                g_fpOut.write(reinterpret_cast<char*>(pBitStream), size);
            }
#endif
            pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Dts, m_ptsList.back());
            const NV_ENC_LOCK_BITSTREAM* pLockBitstreamData = pBitStreamObj->getBitStreamParams();
            if (pLockBitstreamData)
            {
                pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Pts, pLockBitstreamData->outputTimeStamp + m_timeStampDiff);
            }
            else
            {
                pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Pts, m_ptsList.back());
            }

            m_ptsList.pop_back();
            uint64_t dts, pts;
            dts = pBitStreamObj->getPts(eNVENC_TimeStampType::NVENC_TimeStamp_Dts);
            pts = pBitStreamObj->getPts(eNVENC_TimeStampType::NVENC_TimeStamp_Pts);

            if (pBitStreamObj->isKeyFrame())
            {
                DBGMSG(dbgPROFILE, L"%s KeyFrame recieved - Frame No - 0x%x, Size - 0x%x", WFUNCTION, m_frameCnt, size);
            }
            DBGMSG(dbgPROFILE, L"%s Returning bitstream, PTS value - %d, DTS value - %d, bitstream size - %x", WFUNCTION, (int)pts, (int)dts, size);
        }
    }
    return hr;
} //Encode

//-------------------------------------------------------------------
// nvEncBroadcastObj::GetSequenceParams
//-------------------------------------------------------------------
HRESULT nvEncBroadcastObj::GetSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    if (!pBuffer)
    {
        return E_INVALIDARG;
    }

    if (!m_bNvEncInitialized || !m_pEncodeImpl.get())
    {
        return E_NOT_SET;
    }

    HRESULT hr = S_OK;

    hr = m_pEncodeImpl.get()->getSequenceParams(pBuffer);
    if (!SUCCEEDED(hr))
    {
        DBGMSG(dbgERROR, L"%s - Getting Sequence params failed with %x error", WFUNCTION, hr);
    }
    return hr;
} //GetSequenceParams

//-------------------------------------------------------------------
// nvEncBroadcastObj::CreateInstance
//-------------------------------------------------------------------
nvEncBroadcastObj* nvEncBroadcastObj::CreateInstance(NVENC_EncodeCreateParams* pCreateParams)
{
    nvEncBroadcastObj* pCaptureObj = nullptr;
    if (!pCreateParams)
        return NULL;

    pCaptureObj = new nvEncBroadcastObj(pCreateParams);
    if (!pCaptureObj)
    {
        return NULL;
    }
    return pCaptureObj;
} //CreateInstance

//-------------------------------------------------------------------
// nvEncBroadcastObj::createBitstreamBuffer
//-------------------------------------------------------------------
eNVENC_RetCode NVENCBROADCAST_Func nvEncBroadcastObj::createBitstreamBuffer(INVENC_EncodeBitstreamBuffer** ppBuffer)
{
    if (!ppBuffer)
    {
        return API_ERR_INVALID_PARAMETERS;
    }
    INVENC_EncodeBitstreamBuffer* pBuffer = nvEncBroadcastBitstreamBufferObject<uint8_t>::CreateInstance();
    if (pBuffer)
    {
        *ppBuffer = pBuffer;
        return API_SUCCESS;
    }
    return API_ERR_OUT_OF_MEMORY;
} //createBitstreamBuffer

//-------------------------------------------------------------------
// nvEncBroadcastObj::initialize
//-------------------------------------------------------------------
eNVENC_RetCode nvEncBroadcastObj::initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    return SUCCEEDED(Initialize(pEncodeParams, pEncodeSettingsParams)) ? API_SUCCESS : API_ERR_GENERIC;
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastObj::encode
//-------------------------------------------------------------------
eNVENC_RetCode nvEncBroadcastObj::encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    tictoc timer1;
    HRESULT hr = Encode(pEncodeInfo, pBuffer);
    double elapsedTime = timer1.getelapsed();
    DBGMSG(dbgPROFILE, L"%s - time-taken for encode at frame count - %x:  %lf mecs", WFUNCTION, m_frameCnt, elapsedTime);
    return SUCCEEDED(hr) ? API_SUCCESS : API_ERR_GENERIC;
} //encode

//-------------------------------------------------------------------
// nvEncBroadcastObj::getSequenceParams
//-------------------------------------------------------------------
eNVENC_RetCode NVENCBROADCAST_Func nvEncBroadcastObj::getSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return SUCCEEDED(GetSequenceParams(pBuffer)) ? API_SUCCESS : API_ERR_GENERIC;
} //getSequenceParams

//-------------------------------------------------------------------
// nvEncBroadcastObj::finalize
//-------------------------------------------------------------------
eNVENC_RetCode nvEncBroadcastObj::finalize(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    HRESULT hr = S_OK;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (pBitStreamObj)
    {
        hr = m_pEncodeImpl.get()->finalize(pBuffer);
        if (!SUCCEEDED(hr))
        {
            return API_ERR_GENERIC;
        }
        uint32_t size;
        uint8_t* pBitStream = pBitStreamObj->getBitStreamBuffer(&size);
        if (pBitStream && size)
        {
#ifdef TEST_FILE
            if (g_fpOut.is_open())
            {
                g_fpOut.write(reinterpret_cast<char*>(pBitStream), size);
            }
#endif
            if (!m_ptsList.empty())
            {
                pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Dts, m_ptsList.back());
                const NV_ENC_LOCK_BITSTREAM* pLockBitstreamData = pBitStreamObj->getBitStreamParams();
                if (pLockBitstreamData && pLockBitstreamData->outputTimeStamp)
                {
                    pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Pts, pLockBitstreamData->outputTimeStamp + m_timeStampDiff);
                }
                else
                {
                    pBitStreamObj->setPts(eNVENC_TimeStampType::NVENC_TimeStamp_Pts, m_ptsList.back());
                }
                m_ptsList.pop_back();
            }
        }
        m_ptsList.clear();
    }
#ifdef TEST_FILE
    if (g_fpOut.is_open())
    {
        g_fpOut.close();
    }
#endif
    DBGMSG(dbgINFO, L"%s - Encode complete, Frames encoded - 0x%x", WFUNCTION, m_frameCnt);
    return API_SUCCESS;
} //finalize

//-------------------------------------------------------------------
// nvEncBroadcastObj::isInitalized
//-------------------------------------------------------------------
bool nvEncBroadcastObj::isInitalized() const
{
    return m_bNvEncInitialized == true;
} //isInitalized

//-------------------------------------------------------------------
// nvEncBroadcastObj::releaseObject
//-------------------------------------------------------------------
eNVENC_RetCode nvEncBroadcastObj::releaseObject()
{
    --m_RefCount;

    assert(m_RefCount == 0);
    delete this;

    return API_SUCCESS;
} //releaseObject