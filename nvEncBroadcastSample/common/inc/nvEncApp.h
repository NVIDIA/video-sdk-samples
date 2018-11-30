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
#include <memory>
#include <cassert>
#include <shlwapi.h>
#include <wrl.h>
#include "nvEncBroadcastEncodeApi.h"
#include "nvEncAppUtils.h"

using namespace nvEncBroadcastApi;

#ifndef FAILED_NVENC_BROADCASTAPI
#define FAILED_NVENC_BROADCASTAPI(retCode) ((retCode) != API_SUCCESS)
#endif

#ifndef SUCCEEDED_NVENC_BROADCASTAPI
#define SUCCEEDED_NVENC_BROADCASTAPI(retCode) ((retCode) == API_SUCCESS)
#endif

#define DEFAULT_WIDTH  1920
#define DEFAULT_HEIGHT 1080

//todo: Probably might be better to read from a config file
#define   ENCODER_SETTINGS_LENGTH   64
#define   DEFAULT_OUTPUT_FPS_NUM    30000   //Defaulting to 30 fps
#define   DEFAULT_OUTPUT_FPS_DEN    1000
#define   DEFAULT_BITRATE           6000
#define   DEFAULT_CONSTQUALITY      20
#define   DEFAULT_ENCODE_BFRAMES    2

#define   MAX_ENCODE_BFRAMES        3
#define   MAX_BITRATE               40000
#define   MAX_LOOKAHEAD_BUFFERS     6    //currently keeping it a hardcoded value of 6

// This is as an app specific sturcture to hold its settings for the nvEnc session.
// The variables are associated with specific types
// for example, preset is assumed to be "medium", "slow", fast or fastest..This is decided by eNVENC_EncParamsPreset enum in nvEncBroadcastEncodeApi.h
// profile is assumed to be "main", "baseline", high. This is decided by eNVENC_EncParamsProfile enum in nvEncBroadcastEncodeApi.h
// rateControl is assumed to be "main", "baseline", high. This is decided by eNVENC_EncParamsRateControl enum in nvEncBroadcastEncodeApi.h
// Thse could be application implementations on how it wants to map them to the values defined in nvEncBroadcastEncodeApi.h
struct nvEncSettings
{
    uint32_t            bitrate;      // bitrate in Kb
    uint32_t            maxBitrate;   // max bitrate in Kb
    uint32_t            keyintSec;    // keyframe interval in seconds
    char                preset[ENCODER_SETTINGS_LENGTH];   //preset string, to determine eNVENC_EncParamsPreset value to be programmed
    char                profile[ENCODER_SETTINGS_LENGTH];  //profile string, to determine eNVENC_EncParamsProfile value to be programmed
    char                rateControl[ENCODER_SETTINGS_LENGTH]; //preset string, to determine eNVENC_EncParamsRateControl value to be programmed
    uint32_t            bFrames;     // number of b frames, max B frames is 3
    uint32_t            constantQuality;   // constant quality mode value, usually between 14 and 20
    uint32_t            frameRateNum;      // frame rate num
    uint32_t            frameRateDen;      // frame rate denominator
    nvEncBroadcastApi::NVENC_AdvancedEncodeParams advParams;                   // advanced params.
};

nvEncSettings defaultEncoderSettings = {
    DEFAULT_BITRATE,      
    DEFAULT_BITRATE,
    2,
    "medium",
    "high",
    "CBR",
    DEFAULT_ENCODE_BFRAMES,
    DEFAULT_CONSTQUALITY,
    DEFAULT_OUTPUT_FPS_NUM,
    DEFAULT_OUTPUT_FPS_DEN,
    {
        1,  //Enable Spatial AQ by default
        0,
        0,
        MAX_LOOKAHEAD_BUFFERS
    }
};

class nvEncApp
{
protected:
    INvEncDxInterop*     m_pGraphicsContext;
    HANDLE               m_sharedHandle;
    INVENC_EncodeApiObj* m_pEncodeObj;
    OSVERSIONINFO        OsVer;
    nvEncBroadcastApi::NVENC_EncodeSettingsParams m_encodeSettingsParams;
    //-------------------------------------------------------------------
    // IsWin7
    //-------------------------------------------------------------------
    bool IsWin7()
    {
        bool bIsWin7 = false;
        if ((OsVer.dwMajorVersion == 6) && (OsVer.dwMinorVersion == 1))
        {
            bIsWin7 = true;
        }
        return bIsWin7;
    } //IsWin7

    //-------------------------------------------------------------------
    // Release
    //-------------------------------------------------------------------
    void Release()
    {
        if (m_pEncodeObj)
        {
            m_pEncodeObj->releaseObject();
            m_pEncodeObj = nullptr;
        }
        ReleaseGraphicsContext();
    } //Release

    //-------------------------------------------------------------------
    // InitializeGraphicsContext
    //-------------------------------------------------------------------
    bool InitializeGraphicsContext(INvEncDxInterop** ppGraphicsContext)
    {
        std::unique_ptr<INvEncDxInterop> pGraphicsContext = nullptr;
        DxContextType dxType = DxContextType::eDx11Type;
        bool bRet = false;

        if (IsWin7())
        {
            dxType = DxContextType::eDx9Type;
        }

        pGraphicsContext = CNvEncDxInteropFactory::CreateDxInterop(dxType);

        if (pGraphicsContext != nullptr)
        {
            *ppGraphicsContext = pGraphicsContext.release();
            return true;
        }
        return false;
    } //InitializeGraphicsContext

    //-------------------------------------------------------------------
    // initializeEncoderSettings
    //-------------------------------------------------------------------
    void initializeEncoderSettings(const nvEncSettings* pEncSettings, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
    {
        if (!pEncodeSettingsParams || !pEncSettings)
            return;

        memset(pEncodeSettingsParams, 0, sizeof(NVENC_EncodeSettingsParams));

        //TODO: to handle the if else better
        pEncodeSettingsParams->frameRateNum = pEncSettings->frameRateNum;
        pEncodeSettingsParams->frameRateDen = pEncSettings->frameRateDen;
        pEncodeSettingsParams->bFrames = pEncSettings->bFrames;
        pEncodeSettingsParams->bitrate = pEncSettings->bitrate;
        pEncodeSettingsParams->maxBitrate = pEncSettings->maxBitrate;
        if (pEncSettings->keyintSec > 0)
        {
            pEncodeSettingsParams->keyFrameInterval = pEncSettings->keyintSec * pEncSettings->frameRateNum / pEncSettings->frameRateDen;
        }
        pEncodeSettingsParams->constantQuality = pEncSettings->constantQuality;

        //The individual params from advParams are copied one by one for ensuring checks...logically, the entire struct can be copied as is.
        pEncodeSettingsParams->advParams.enableLookAhead = pEncSettings->advParams.enableLookAhead;
        if (pEncodeSettingsParams->advParams.enableLookAhead == 1)
        {
            pEncodeSettingsParams->advParams.loopAheadDepth = pEncSettings->advParams.loopAheadDepth;
            if (pEncodeSettingsParams->advParams.loopAheadDepth > MAX_LOOKAHEAD_BUFFERS)
                pEncodeSettingsParams->advParams.loopAheadDepth = MAX_LOOKAHEAD_BUFFERS;
        }
        pEncodeSettingsParams->advParams.enablePSY_AQ = pEncSettings->advParams.enablePSY_AQ;

        if (!_stricmp(pEncSettings->profile, "baseline"))
            pEncodeSettingsParams->encProfile = eNVENC_EncParamsProfile::NVENC_EncParams_Profile_Baseline;
        else if (!_stricmp(pEncSettings->profile, "main"))
            pEncodeSettingsParams->encProfile = eNVENC_EncParamsProfile::NVENC_EncParams_Profile_Main;
        else if (!_stricmp(pEncSettings->profile, "high"))
            pEncodeSettingsParams->encProfile = eNVENC_EncParamsProfile::NVENC_EncParams_Profile_High;
        else
            pEncodeSettingsParams->encProfile = eNVENC_EncParamsProfile::NVENC_EncParams_Profile_Auto;

        if (!_stricmp(pEncSettings->rateControl, "VBR"))
            pEncodeSettingsParams->rateControl = eNVENC_EncParamsRateControl::NVENC_EncParams_RC_VBR;
        else if (!_stricmp(pEncSettings->rateControl, "CBR"))
            pEncodeSettingsParams->rateControl = eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CBR;
        else
            pEncodeSettingsParams->rateControl = eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CONSTQUAL;

        if (!_stricmp(pEncSettings->preset, "medium") || !_stricmp(pEncSettings->preset, "default"))
            pEncodeSettingsParams->preset = eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Default;
        else if (!_stricmp(pEncSettings->preset, "slow"))
            pEncodeSettingsParams->preset = eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Slow;
        else if (!_stricmp(pEncSettings->preset, "fast"))
            pEncodeSettingsParams->preset = eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Fast;
        else
            pEncodeSettingsParams->preset = eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Fastest;
    } //initializeEncoderSettings

    //-------------------------------------------------------------------
    // ReleaseGraphicsContext
    //-------------------------------------------------------------------
    void ReleaseGraphicsContext()
    {
        if (m_pGraphicsContext)
        {
            delete m_pGraphicsContext;
            m_pGraphicsContext = nullptr;
        }
    } //ReleaseGraphicsContext

public:
    nvEncApp()
        : m_pGraphicsContext(nullptr)
        , m_sharedHandle(nullptr)
        , m_pEncodeObj(nullptr)
    {
        memset(&OsVer, 0, sizeof(OSVERSIONINFO));
        OsVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&OsVer);

        memset(&m_encodeSettingsParams, 0, sizeof(NVENC_EncodeSettingsParams));
    }

    virtual ~nvEncApp() {
        Release();
    } //destructor

    //-------------------------------------------------------------------
    // InitializeInputParams
    //-------------------------------------------------------------------
    bool InitializeInputParams(const AppEncodeParams& g_Params, NVENC_EncodeInitParams& initParams, uint32_t& nSize)
    {
        initParams.width = DEFAULT_WIDTH;
        initParams.height = DEFAULT_HEIGHT;
        std::wstring inputName = g_Params.inputFile;
        std::wstring outputName = g_Params.outputFile;

        //get the extension and format determined based on that
        std::size_t lastindex = inputName.find_last_of(L".");
        std::wstring extName = inputName.substr(lastindex);
        if (0 == _wcsicmp(extName.c_str(), L".yuv"))
        {
            initParams.bufferFormat = kBufferFormat_NV12;
            nSize = (initParams.width * initParams.height * 3) / 2;
        }
        else if (0 == _wcsicmp(extName.c_str(), L".rgb") ||
            0 == _wcsicmp(extName.c_str(), L".argb")
            )
        {
            initParams.bufferFormat = kBufferFormat_ARGB;
            nSize = initParams.width * initParams.height * 4;
        }
        else
        {
            return false;
        }

        if ((g_Params.width && g_Params.width != initParams.width) || (g_Params.height && g_Params.height != initParams.height))
        {
            initParams.width = g_Params.width;
            initParams.height = g_Params.height;
        }
        return true;
    } //InitializeInputParams

    //-------------------------------------------------------------------
    // SetFrameParams
    //-------------------------------------------------------------------
    bool SetFrameParams(nvEncBroadcastApi::NVENC_EncodeInfo& encodeInfo, const nvEncBroadcastApi::NVENC_EncodeInitParams& initParams, const uint8_t* pSrc)
    {
        if (!pSrc)
        {
            return false;
        }
        bool bRet = m_pGraphicsContext->SetFrameParams(encodeInfo, initParams, pSrc);
        return bRet;
    } //SetFrameParams

    //-------------------------------------------------------------------
    // InitializeNvEncContext
    //-------------------------------------------------------------------
    bool InitializeNvEncContext(const AppEncodeParams& g_Params, NVENC_EncodeInitParams& initParams, uint32_t& nSize)
    {
        bool bRet = InitializeGraphicsContext(&m_pGraphicsContext);
        if (!bRet)
        {
            return false;
        }

        eNVENC_RetCode retCode = API_SUCCESS;

        NVENC_EncodeCreateParams createParams;
        memset(&createParams, 0, sizeof(NVENC_EncodeCreateParams));

        //first try the cancreate flag before proceeding
        createParams.size = sizeof(NVENC_EncodeCreateParams);
        createParams.version = NVENC_ENCODEAPI_VER1;
        createParams.createFlags = NVENC_EncodeCanCreate_Flag;

        if (FAILED_NVENC_BROADCASTAPI(NVENC_EncodeInitialize(&createParams, nullptr)))
        {
            std::cout << "EncodeInitialize call in nvEnc API failed" << std::endl;
            return false;
        }

        if (!InitializeInputParams(g_Params, initParams, nSize))
        {
            goto exit;
        }

        bRet = m_pGraphicsContext->Initialize(&initParams);
        if (!bRet)
        {
            goto exit;
        }

        //now initialize and get the actual api object
        createParams.size = sizeof(NVENC_EncodeCreateParams);
        createParams.version = NVENC_ENCODEAPI_VER1;
        retCode = NVENC_EncodeInitialize(&createParams, &m_pEncodeObj);
        if (FAILED_NVENC_BROADCASTAPI(retCode) || !m_pEncodeObj)
        {
            return false;
        }

        //now initialize the encoder settings based on the default settings...These can be changed based on how an app can change based on its own settings
        // by keeping a copy of the default and updating them based on its usage
        initializeEncoderSettings(&defaultEncoderSettings, &m_encodeSettingsParams);

        //Now initialize the nvEnc api with the buffer settings and the encode params.
        retCode = m_pEncodeObj->initialize(&initParams, &m_encodeSettingsParams);
        if (FAILED_NVENC_BROADCASTAPI(retCode))
        {
            goto exit;
        }
        return true;
    exit:
        return false;
    } //InitializeNvEncContext

    //-------------------------------------------------------------------
    // LoadBMP
    //-------------------------------------------------------------------
    BYTE* LoadBMP(UINT32* width, UINT32* height, UINT32* size, BITMAPINFOHEADER& bmpinfo, LPCTSTR bmpfile)
    {
        BITMAPFILEHEADER bmpheader;
        DWORD bytesread;

        HANDLE file = CreateFile(bmpfile, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (NULL == file)
            return nullptr;

        if (ReadFile(file, &bmpheader, sizeof(BITMAPFILEHEADER),
            &bytesread, NULL) == false)
        {
            CloseHandle(file);
            return nullptr;
        }

        if (ReadFile(file, &bmpinfo, sizeof(BITMAPINFOHEADER),
            &bytesread, NULL) == false)
        {
            CloseHandle(file);
            return nullptr;
        }
        if (bmpheader.bfType != 'MB')
        {
            CloseHandle(file);
            return nullptr;
        }

        if (bmpinfo.biCompression != BI_RGB)
        {
            CloseHandle(file);
            return nullptr;
        }

        if (width)
        {
            *width = bmpinfo.biWidth;
        }
        if (height)
        {
            *height = abs(bmpinfo.biHeight);
        }
        if (size)
        {
            *size = bmpheader.bfSize - bmpheader.bfOffBits;
        }

        BYTE* pBuffer = new BYTE[*size];
        if (!pBuffer)
        {
            return nullptr;
        }
        SetFilePointer(file, bmpheader.bfOffBits, NULL, FILE_BEGIN);
        if (ReadFile(file, pBuffer, *size, &bytesread, NULL) == false)
        {
            delete[] pBuffer;
            CloseHandle(file);
            return nullptr;
        }
        CloseHandle(file);
        return pBuffer;
    } //LoadBMP

    INVENC_EncodeApiObj* GetNvEncObj() const {
        return m_pEncodeObj;
    } //GetNvEncObj
};