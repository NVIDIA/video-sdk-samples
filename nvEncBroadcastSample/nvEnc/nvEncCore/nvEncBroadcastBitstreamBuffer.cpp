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
#include "nvEncBroadcastBitstreamBuffer.h"
#include "nvEncBroadcastBitstreamBufferInst.h"
#include <cassert>

using namespace nvEncBroadcastApi;

template <typename T, class Alloc>
nvEncBroadcastBitstreamBufferObject<T, Alloc>::nvEncBroadcastBitstreamBufferObject()
           : m_bBufferInitialized(false)
           , m_pData(nullptr)
           , m_DataSize(0)
           , m_RefCount(0)

{
    memset(&m_bitStreamData, 0, sizeof(m_bitStreamData));
    memset(m_ptsArray, 0, sizeof(m_ptsArray));
    ++m_RefCount;
} //constructor

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::~nvEncBroadcastBitstreamBufferObject
//-------------------------------------------------------------------
template <typename T, class Alloc>
nvEncBroadcastBitstreamBufferObject<T, Alloc>::~nvEncBroadcastBitstreamBufferObject()
{
    Release();
} //destructor

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::Initialize
//-------------------------------------------------------------------
template <typename T, class Alloc>
HRESULT nvEncBroadcastBitstreamBufferObject<T, Alloc>::Initialize()
{
    Release();
    m_bBufferInitialized = true;
    return S_OK;
} //Initialize

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::Recycle
//-------------------------------------------------------------------
template <typename T, class Alloc>
HRESULT nvEncBroadcastBitstreamBufferObject<T, Alloc>::Recycle()
{
    Release();
    m_bBufferInitialized = true;
    return S_OK;
} //Recycle

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::Release
//-------------------------------------------------------------------
template <typename T, class Alloc>
HRESULT nvEncBroadcastBitstreamBufferObject<T, Alloc>::Release()
{
    if (m_pData)
    {
        Allocator<uint8_t>::deallocate(m_pData, m_DataSize, true);
        m_pData = nullptr;
        m_DataSize = 0;
    }

    m_DataSize = 0;
    m_bBufferInitialized = false;
    memset(m_ptsArray, 0, sizeof(m_ptsArray));
    memset(&m_bitStreamData, 0, sizeof(m_bitStreamData));
    return S_OK;
} //Release


//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::CreateInstance
//-------------------------------------------------------------------
template <typename T, class Alloc>
nvEncBroadcastBitstreamBufferObject<T, Alloc>* nvEncBroadcastBitstreamBufferObject<T, Alloc>::CreateInstance()
{
    nvEncBroadcastBitstreamBufferObject* pBufferObj = nullptr;

    pBufferObj = new nvEncBroadcastBitstreamBufferObject<T, Alloc>();
    if (!pBufferObj)
    {
        return NULL;
    }
    return pBufferObj;
} //CreateInstance

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::initialize
//-------------------------------------------------------------------
template <typename T, class Alloc>
eNVENC_RetCode nvEncBroadcastBitstreamBufferObject<T, Alloc>::initialize()
{
    return SUCCEEDED(Initialize()) ? API_SUCCESS : API_ERR_GENERIC;
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::getBitStreamBuffer
//-------------------------------------------------------------------
template <typename T, class Alloc>
uint8_t* nvEncBroadcastBitstreamBufferObject<T, Alloc>::getBitStreamBuffer(uint32_t* size) const
{
    if (!size)
    {
        return nullptr;
    }

    if (m_pData)
    {
        *size = m_DataSize;
        return m_pData;
    }
    return nullptr;
} //getBitStreamBuffer

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::isInitalized
//-------------------------------------------------------------------
template <typename T, class Alloc>
bool nvEncBroadcastBitstreamBufferObject<T, Alloc>::isInitalized() const
{
    return m_bBufferInitialized == true;
} //isInitalized

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::release
//-------------------------------------------------------------------
template <typename T, class Alloc>
eNVENC_RetCode nvEncBroadcastBitstreamBufferObject<T, Alloc>::release()
{
    --m_RefCount;

    assert(m_RefCount == 0);
    delete this;

    return API_SUCCESS;
} //release

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::recycle
//-------------------------------------------------------------------
template <typename T, class Alloc>
eNVENC_RetCode nvEncBroadcastBitstreamBufferObject<T, Alloc>::recycle()
{
    return SUCCEEDED(Recycle()) ? API_SUCCESS : API_ERR_GENERIC;
} //recycle

//-------------------------------------------------------------------
// nvEncBroadcastBitstreamBufferObject::isKeyFrame
//-------------------------------------------------------------------
template <typename T, class Alloc>
bool nvEncBroadcastBitstreamBufferObject<T, Alloc>::isKeyFrame() const
{
    return (m_bitStreamData.pictureType == NV_ENC_PIC_TYPE_IDR);
} //isKeyFrame