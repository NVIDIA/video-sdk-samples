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

#include <cassert>

#include "nvEncBroadcastInterfaceImplD3D9.h"
#include "nvEncBroadcastBitstreamBuffer.h"
#include "nvEncBroadcastUtils.h"

using namespace nvEncBroadcastApi;
using namespace nvEncBroadcastUtils;

//static variables
nvEncBroadcastDirectXContext nvEncBroadcastDirectXContext::context;

//-------------------------------------------------------------------
// nvEncBroadcastDirectXContext::instance
//-------------------------------------------------------------------
nvEncBroadcastDirectXContext* nvEncBroadcastDirectXContext::instance()
{
    return &context;
} //instance

//-------------------------------------------------------------------
// nvEncBroadcastDirectXContext::Create
//-------------------------------------------------------------------
bool nvEncBroadcastDirectXContext::Create(uint32_t width, uint32_t height)
{
    bool                 rc = false;
    LPDIRECT3D9EX        pD3D9 = NULL;
    LPDIRECT3DDEVICE9EX  pD3DDev = NULL;
    LPDIRECT3DDEVICE9EX  pSchedulerD3DDev = NULL;
    bool                 bIsCOMInit = false;
    HRESULT              hr = S_OK;
    IDirectXVideoAccelerationService *pDXVA = NULL;
    IDirectXVideoProcessorService *pDXVAProc = NULL;

    // Try creating the DX device. If successful, release and replace the previous device.
    do
    {
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr))
        {
            bIsCOMInit = true;
        }

        hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D9);
        if (FAILED(hr) || !pD3D9)
        {
            DBGMSG(dbgERROR, L"%s -Direct3DCreate9Ex failed with %x error", WFUNCTION, hr);
            break;
        }

        // Set up the structure used to create the D3DDevice. Since we are now
        // using more complex geometry, we will create a device with a zbuffer.
        D3DPRESENT_PARAMETERS d3dpp;
        ZeroMemory(&d3dpp, sizeof(d3dpp));
        d3dpp.Windowed = TRUE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferWidth = width;
        d3dpp.BackBufferHeight = height;
        d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
        d3dpp.EnableAutoDepthStencil = TRUE;
        d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        if (SUCCEEDED(hr))
        {
            hr = pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetDesktopWindow(),
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                &d3dpp, NULL, &pD3DDev);
            // WAR: Some Win10 systems allow only one device to be created on one window
            if (hr == E_ACCESSDENIED)
            {
                 DBGMSG(dbgERROR, L"%s -CreateDeviceEx failed with %x error", WFUNCTION, hr);
            }
            m_width = width;
            m_height = height;
        }
        if (FAILED(hr))
        {
            //LogD3DERR(hr);
            DBGMSG(dbgERROR, L"%s -CreateDeviceEx failed with %x error", WFUNCTION, hr);
            pD3DDev = NULL;
            break;
        }
        // Turn off culling
        pD3DDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        // Turn off D3D lighting
        //pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);

        hr = DXVA2CreateVideoService(pD3DDev, __uuidof(IDirectXVideoAccelerationService), (void **)&pDXVA);
        if (FAILED(hr))
        {
            DBGMSG(dbgERROR, L"%s -DXVA2CreateVideoService for IDirectXVideoAccelerationService device failed with %x error", WFUNCTION, hr);
            break;
        }

        hr = DXVA2CreateVideoService(pD3DDev, __uuidof(IDirectXVideoProcessorService), (void **)&pDXVAProc);
        if (FAILED(hr))
        {
            DBGMSG(dbgERROR, L"%s -DXVA2CreateVideoService for IDirectXVideoProcessorService device failed with %x error", WFUNCTION, hr);
            break;
        }
        rc = true;
    } while (0);

    if (rc)
    {
        // Device creation successful. Release and replace the earlier device.
        Release();
        m_bIsCOMRuntimeInitialized = bIsCOMInit;
        m_pDirect3D9 = pD3D9;
        m_pDirect3DDevice9 = pD3DDev;
        m_pDXVA = pDXVA;
        m_pDXVAProc = pDXVAProc;
    }
    else
    {
        // Device cannot be created at this point of time. Don't release previous device.
        if (pDXVA)
        {
            pDXVA->Release();
            pDXVA = NULL;
        }
        if (pDXVAProc)
        {
            pDXVAProc->Release();
            pDXVAProc = NULL;
        }
        if (pD3DDev)
        {
            pD3DDev->Release();
            pD3DDev = NULL;
        }
        if (pSchedulerD3DDev)
        {
            pSchedulerD3DDev->Release();
            pSchedulerD3DDev = NULL;
        }
        if (pD3D9)
        {
            pD3D9->Release();
            pD3D9 = NULL;
        }
        if (bIsCOMInit)
        {
            CoUninitialize();
        }
    }
    return rc;
} //Create

//-------------------------------------------------------------------
// nvEncBroadcastDirectXContext::Release
//-------------------------------------------------------------------
void nvEncBroadcastDirectXContext::Release()
{
    if (m_pDXVA)
    {
        m_pDXVA->Release();
        m_pDXVA = NULL;
    }
    if (m_pDXVAProc)
    {
        m_pDXVAProc->Release();
        m_pDXVAProc = NULL;
    }

    if (m_pDirect3DDevice9)
    {
        m_pDirect3DDevice9->Release();
        m_pDirect3DDevice9 = NULL;
    }
    if (m_pDirect3D9)
    {
        m_pDirect3D9->Release();
        m_pDirect3D9 = NULL;
    }
    if (m_bIsCOMRuntimeInitialized)
    {
        CoUninitialize();
        m_bIsCOMRuntimeInitialized = false;
    }
} //Release

//nvEncBroadcastInterfaceImplD3D9 - this is the actual D3D9 impl class..above is just a simple singleton.
//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::nvEncBroadcastInterfaceImplD3D9
//-------------------------------------------------------------------
nvEncBroadcastInterfaceImplD3D9::nvEncBroadcastInterfaceImplD3D9()
           : INvEncBroadcastInterface()
           , m_bNvEncInterfaceInitialized(false)
           , m_d3dFormat(D3DFMT_A8R8G8B8)
           , m_RefCount(0)

{
    memset(&m_createParams, 0, sizeof(m_createParams));
    m_pEncodeD3D9.reset();
    ++m_RefCount;
} // (Constructor)

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::~nvEncBroadcastInterfaceImplD3D9
//-------------------------------------------------------------------
nvEncBroadcastInterfaceImplD3D9::~nvEncBroadcastInterfaceImplD3D9()
{
    nvEncBroadcastDirectXContext::instance()->Release();
}// destructor

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::Initialize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::Initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    HRESULT hr = S_OK;
    NV_ENC_BUFFER_FORMAT nvEncFormat;

    if (!pEncodeParams)
    {
        return E_INVALIDARG;
    }

    memcpy(&m_createParams, pEncodeParams, sizeof(m_createParams));

    if (!m_bNvEncInterfaceInitialized)
    {
        if (!nvEncBroadcastDirectXContext::instance()->Create())
        {
            return E_FAIL;
        }
    }

    if (SUCCEEDED(hr))
    {
        IDirect3DDevice9 *pDevice = nvEncBroadcastDirectXContext::getDevice();
        IDirectXVideoAccelerationService *pDXVA = nvEncBroadcastDirectXContext::getDXVA();

        if (!pDevice || !pDXVA)
        {
            return E_FAIL;
        }

        memcpy(&m_encodeParams, pEncodeParams, sizeof(m_encodeParams));

        switch (m_encodeParams.bufferFormat)
        {
        case kBufferFormat_ARGB:
            m_d3dFormat = D3DFMT_A8R8G8B8;
            nvEncFormat = static_cast<NV_ENC_BUFFER_FORMAT>(0x01000000);
            break;
        case kBufferFormat_NV12:
            m_d3dFormat = D3DFMT_NV12;
            nvEncFormat = NV_ENC_BUFFER_FORMAT_NV12_PL;
            break;
        default:
            m_d3dFormat = D3DFMT_A8R8G8B8;
            nvEncFormat = static_cast<NV_ENC_BUFFER_FORMAT>(0x01000000);
            break;
        }
        hr = pDevice->CreateOffscreenPlainSurface(m_encodeParams.width, m_encodeParams.height, m_d3dFormat, D3DPOOL_DEFAULT, &m_pSurfaceBgra, NULL);
        if (!SUCCEEDED(hr) || !m_pSurfaceBgra.Get())
        {
            DBGMSG(dbgERROR, L"%s: CreateOffscreenPlainSurface failed with %x error", WFUNCTION, hr);
            return hr;
        }

        try
        {
            m_pEncodeD3D9.reset(new NvEncoderD3D9(pDevice, m_encodeParams.width, m_encodeParams.height, nvEncFormat, (m_d3dFormat == D3DFMT_NV12) ? nullptr : nullptr));
        }
        catch (std::exception& e)
        {
            DBGMSG(dbgERROR, L"%s: creating NvEncoderD3D9 object threw exception : %s ", WFUNCTION, e.what());
            hr = E_FAIL;
        }
        catch (...)
        {
            DBGMSG(dbgERROR, L"%s: creating NvEncoderD3D9 object threw unhandled exception", WFUNCTION);
            hr = E_FAIL;
        }
        if (SUCCEEDED(hr))
        {
            try
            {
                NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
                NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
                initializeParams.encodeConfig = &encodeConfig;
                hr = INvEncBroadcastInterface::initializeEncodeConfig(m_pEncodeD3D9.get(), &initializeParams, pEncodeSettingsParams);
                if (!SUCCEEDED(hr))
                {
                    DBGMSG(dbgERROR, L"%s - initializing encode config params failed with error - %x\n", WFUNCTION, hr);
                    return hr;
                }
                m_pEncodeD3D9.get()->CreateEncoder(&initializeParams);
            }
            catch (std::exception& e)
            {
                DBGMSG(dbgERROR, L"%s: Initializing NvEncoderD3D9 with params threw exception : %s ", WFUNCTION, e.what());
                hr = E_FAIL;
            }
            catch (...)
            {
                DBGMSG(dbgERROR, L"%s: Initializing NvEncoderD3D9 with params threw unhandled exception", WFUNCTION);
                hr = E_FAIL;
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        m_bNvEncInterfaceInitialized = true;
    }
    return hr;
} //Initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::Release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::Release()
{
    --m_RefCount;

    assert(m_RefCount == 0);
    delete this;

    return API_SUCCESS;
} //Release

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::Encode
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::Encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    if (!pEncodeInfo || !pBuffer)
    {
        return E_INVALIDARG;
    }

    std::vector<std::vector<uint8_t>> vPacket;
    uint32_t bitStreamSize = 0;
    HRESULT hr = S_OK;
    NV_ENC_PIC_PARAMS picParams;
    NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };

    switch (pEncodeInfo->bufferInfo.bufferType)
    {
    case kBufferType_Vid:
        return E_INVALIDARG;
        break;

    case kBufferType_Sys:
        if (pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer || pEncodeInfo->bufferInfo.DxBuffer.bufferHandle)
        {
            const NvEncInputFrame* encoderInputFrame = m_pEncodeD3D9.get()->GetNextInputFrame();
            if (!encoderInputFrame)
            {
                DBGMSG(dbgINFO, L"%s - GetNextInputFrame return null frame, queue probably full", WFUNCTION);
                return S_OK; //returning S_OK so that the encoder in the pipeline doesnt stop processing, essentially frame dropped
            }
            IDirect3DSurface9 *pSurface = reinterpret_cast<IDirect3DSurface9*>(encoderInputFrame->inputPtr);
            assert(pSurface);

            D3DLOCKED_RECT lockedRect;
            hr = pSurface->LockRect(&lockedRect, NULL, 0);
            if (SUCCEEDED(hr))
            {
                switch (m_encodeParams.bufferFormat)
                {
                case kBufferFormat_ARGB:
                {
                    for (unsigned int y = 0; y < m_encodeParams.height; y++)
                    {
                        memcpy((uint8_t *)lockedRect.pBits + y * lockedRect.Pitch, (uint8_t *)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer + y * pEncodeInfo->bufferInfo.SysBuffer.lineWidth * 4), m_encodeParams.width * 4);
                    }
                }
                break;
                case kBufferFormat_NV12:
                {
                    for (unsigned int y = 0; y < m_encodeParams.height; y++)
                    {
                        memcpy((uint8_t *)lockedRect.pBits + y * lockedRect.Pitch, (uint8_t *)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer + y * pEncodeInfo->bufferInfo.SysBuffer.lineWidth), m_encodeParams.width * 1);
                    }

                    uint8_t* pUVDest = reinterpret_cast<uint8_t*>((uint64_t)(lockedRect.pBits) + lockedRect.Pitch* m_encodeParams.height);
                    uint8_t* pSrc = reinterpret_cast<uint8_t*>((uint64_t)(pEncodeInfo->bufferInfo.SysBuffer.pixelBuffer) + m_encodeParams.height * pEncodeInfo->bufferInfo.SysBuffer.lineWidth * 1);
                    for (unsigned int y = 0; y < m_encodeParams.height / 2; y++)
                    {
                        memcpy((uint8_t *)pUVDest, pSrc, m_encodeParams.width);
                        pUVDest += lockedRect.Pitch;
                        pSrc += pEncodeInfo->bufferInfo.SysBuffer.lineWidth;
                    }
                }
                break;
                default:
                    break;
                }
                pSurface->UnlockRect();
            }
            memset(&picParams, 0, sizeof(picParams));
            picParams.version = NV_ENC_PIC_PARAMS_VER;
            picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
            picParams.inputTimeStamp = pEncodeInfo->pts;
            m_pEncodeD3D9.get()->EncodeFrame(vPacket, &picParams, &lockBitstreamData);
        }
        break;
    }

    for (std::vector<uint8_t> &packet : vPacket)
    {
        //since it is a bitstream size, the cast to uint32_t should be ok
        bitStreamSize += static_cast<uint32_t>(packet.size());
    }

    uint8_t* pDest = nullptr;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (bitStreamSize && pBitStreamObj)
    {
        pDest = pBitStreamObj->allocBuffer(bitStreamSize);
    }

    if (pDest)
    {
        uint32_t size = 0;
        for (std::vector<uint8_t> &packet : vPacket)
        {
            memcpy(pDest, packet.data(), packet.size());
            pDest += packet.size();
            //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
            size += static_cast<uint32_t>(packet.size());
            if (size > bitStreamSize)
            {
                assert(0);
                break;
            }
        }
        pBitStreamObj->setBitstreamParams(&lockBitstreamData);
    }
    return S_OK;
} //Encode

//-------------------------------------------------------------------
// nvEncBroadcastObj::Finalize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::Finalize(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    HRESULT hr = S_OK;
    std::vector<std::vector<uint8_t>> vPacket;
    uint32_t bitStreamSize = 0;
    NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };

    m_pEncodeD3D9.get()->EndEncode(vPacket, &lockBitstreamData);

    for (std::vector<uint8_t> &packet : vPacket)
    {
        //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
        bitStreamSize += static_cast<uint32_t>(packet.size());
    }

    uint8_t* pDest = nullptr;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (pBitStreamObj && bitStreamSize)
    {
        pDest = pBitStreamObj->allocBuffer(bitStreamSize);
    }

    if (pDest)
    {
        uint32_t size = 0;
        for (std::vector<uint8_t> &packet : vPacket)
        {
            memcpy(pDest, packet.data(), packet.size());
            pDest += packet.size();
            //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
            size += static_cast<uint32_t>(packet.size());
            if (size > bitStreamSize)
            {
                assert(0);
                hr = ERROR_INCORRECT_SIZE;
                break;
            }
        }
        pBitStreamObj->setBitstreamParams(&lockBitstreamData);
    }
    return hr;
} //Finalize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::GetSequenceParams
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::GetSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    if (!pBuffer)
    {
        return E_INVALIDARG;
    }

    if (!m_bNvEncInterfaceInitialized || !m_pEncodeD3D9.get())
    {
        return E_NOT_SET;
    }

    std::vector<uint8_t> vPacket;
    uint32_t bitStreamSize = 0;

    vPacket.clear();
    m_pEncodeD3D9.get()->GetSequenceParams(vPacket);
    //since it is a bitstream size, the cast to uint32_t should be ok as size should be higher than what a uint32_t can carry
    bitStreamSize += static_cast<uint32_t>(vPacket.size());

    uint8_t* pDest = nullptr;
    nvEncBroadcastBitstreamBufferObject<uint8_t>* pBitStreamObj = static_cast<nvEncBroadcastBitstreamBufferObject<uint8_t>*>(pBuffer);
    if (bitStreamSize && pBitStreamObj)
    {
        pDest = pBitStreamObj->allocBuffer(bitStreamSize);
    }

    if (pDest)
    {
        memcpy(pDest, vPacket.data(), vPacket.size());
    }
    return S_OK;
} //GetSequenceParams

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::CreateInstance
//-------------------------------------------------------------------
nvEncBroadcastInterfaceImplD3D9* nvEncBroadcastInterfaceImplD3D9::CreateInstance()
{
    nvEncBroadcastInterfaceImplD3D9* pEncodeObj = nullptr;
    pEncodeObj = new nvEncBroadcastInterfaceImplD3D9();
    if (!pEncodeObj)
    {
        return NULL;
    }
    return pEncodeObj;
} //CreateInstance

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::initialize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::initialize(NVENC_EncodeInitParams* pEncodeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    return Initialize(pEncodeParams, pEncodeSettingsParams);
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::encode
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return Encode(pEncodeInfo, pBuffer);
} //encode

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::finalize
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::finalize(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return Finalize(pBuffer);
} //finalize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::uploadToTexture
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::uploadToTexture()
{
    return S_OK;
    //return ploadToTexture(pBuffer);
} //uploadToTexture

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::nvEncBroadcastInterfaceImplD3D9::release()
{
    return Release();
} //initialize

//-------------------------------------------------------------------
// nvEncBroadcastInterfaceImplD3D9::release
//-------------------------------------------------------------------
HRESULT nvEncBroadcastInterfaceImplD3D9::getSequenceParams(INVENC_EncodeBitstreamBuffer* pBuffer)
{
    return GetSequenceParams(pBuffer);
}  //getSequenceParams