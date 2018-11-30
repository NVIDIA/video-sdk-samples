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

#include "nvEncBroadcastInterface.h"
#include <memory>
#include "nvEncBroadcastInterfaceImplD3D9.h"
#include "nvEncBroadcastInterfaceImplD3D11.h"
#include "nvEncBroadcastUtils.h"

using namespace nvEncBroadcastApi;

//-------------------------------------------------------------------
// namespace CNvEncBroadcastFactory
//-------------------------------------------------------------------
namespace CNvEncBroadcastFactory
{
    //-------------------------------------------------------------------
    // CreateNvEncInstance
    //-------------------------------------------------------------------
    std::unique_ptr<INvEncBroadcastInterface> CreateNvEncInstance(eNvEncType type)
    {
        std::unique_ptr<INvEncBroadcastInterface> pEncode;
        pEncode.reset();
        INvEncBroadcastInterface* pNvEncoder = nullptr;
        switch (type)
        {
        case eNvEncType::eNvEncoderType_DX9:
        {
            pNvEncoder = nvEncBroadcastInterfaceImplD3D9::CreateInstance();
            if (pNvEncoder)
                pEncode.reset(pNvEncoder);
            break;
        }

        case eNvEncType::eNvEncoderType_DX11:
        {
            pNvEncoder = nvEncBroadcastInterfaceImplD3D11::CreateInstance();
            if (pNvEncoder)
                pEncode.reset(pNvEncoder);
            break;
        }

        default:
            DBGMSG(dbgERROR, L"CreateNvEncInstance recieved Invalid - %x encoder type", type);
            pEncode.reset();
            break;
        }
        return pEncode;
    } //CreateNvEncInstance
} //namespace

//-------------------------------------------------------------------
// INvEncBroadcastInterface::validateEncodingParams
//-------------------------------------------------------------------
bool INvEncBroadcastInterface::validateEncodingParams(NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    if (!pEncodeSettingsParams->bitrate && pEncodeSettingsParams->rateControl != eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CONSTQUAL)
    {
        DBGMSG(dbgERROR, L"%s - Invalid bitrate - %x passed with rate conrol set to - %d", WFUNCTION, pEncodeSettingsParams->bitrate, pEncodeSettingsParams->rateControl);
        return false;
    }

    if (pEncodeSettingsParams->bFrames > MAX_ENCODE_BFRAMES)
    {
        DBGMSG(dbgERROR, L"%s - Invalid b frames - %x passed ", WFUNCTION, pEncodeSettingsParams->bFrames);
        return false;
    }

    if (pEncodeSettingsParams->frameRateNum == 0 || pEncodeSettingsParams->frameRateDen == 0)
    {
        DBGMSG(dbgERROR, L"%s - Invalid frame rate number passed, defaulting to 30", WFUNCTION);
        pEncodeSettingsParams->frameRateNum = DEFAULT_OUTPUT_FPS_NUM;
        pEncodeSettingsParams->frameRateDen = DEFAULT_OUTPUT_FPS_DEN;
    }
   
    return true;
} //validateEncodingParams


//TODO: to convert the never ending if-else statements related to encoing params to lambdas
//-------------------------------------------------------------------
// INvEncBroadcastInterface::initializeEncodeConfig
//-------------------------------------------------------------------
HRESULT INvEncBroadcastInterface::initializeEncodeConfig(NvEncoder* pEncodeObj, NV_ENC_INITIALIZE_PARAMS* pInitializeParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams)
{
    HRESULT hr = S_OK;

    //Codec Guid specified to H264 for now
    GUID codecGUID = NV_ENC_CODEC_H264_GUID, presetGUID = NV_ENC_PRESET_DEFAULT_GUID, profileGUID = NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;
    bool bTwoPass = false;

    if (!pInitializeParams)
    {
        DBGMSG(dbgERROR, L"%s - Invalid encode config params passed", WFUNCTION, hr);
        return E_INVALIDARG;
    }

    if (!pEncodeObj)
    {
        DBGMSG(dbgERROR, L"%s - Null encoder object passed", WFUNCTION, hr);
        return E_INVALIDARG;
    }

    if (pEncodeSettingsParams)
    {
        if (!validateEncodingParams(pEncodeSettingsParams))
        {
            DBGMSG(dbgERROR, L"%s - Validation of input params failed", WFUNCTION, hr);
            return E_INVALIDARG;
        }

        //check for profile
        if (pEncodeSettingsParams->encProfile == eNVENC_EncParamsProfile::NVENC_EncParams_Profile_Baseline)
        {
            profileGUID = NV_ENC_H264_PROFILE_BASELINE_GUID;
        }
        else if (pEncodeSettingsParams->encProfile == eNVENC_EncParamsProfile::NVENC_EncParams_Profile_High)
        {
            profileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;
        }
        else if (pEncodeSettingsParams->encProfile == eNVENC_EncParamsProfile::NVENC_EncParams_Profile_Main)
        {
            profileGUID = NV_ENC_H264_PROFILE_MAIN_GUID;
        }
        else
        {
            DBGMSG(dbgERROR, L"%s - No matching profile specified, going with Auto", WFUNCTION, hr);
        }

        //check for preset
        if (pEncodeSettingsParams->preset == eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Fastest)
        {
            presetGUID = NV_ENC_PRESET_HP_GUID;
        }
        else if (pEncodeSettingsParams->preset == eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Fast)
        {
            presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
        }
        else if (pEncodeSettingsParams->preset == eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Medium)
        {
            presetGUID = NV_ENC_PRESET_HQ_GUID;
        }
        else if (pEncodeSettingsParams->preset == eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Slow)
        {
            presetGUID = NV_ENC_PRESET_HQ_GUID;
            bTwoPass = true;
        }
        else        
        {
            DBGMSG(dbgERROR, L"%s - No matching preset specified, going with Auto", WFUNCTION, hr);
        }
    }

    try
    {
        pEncodeObj->CreateDefaultEncoderParams(pInitializeParams, codecGUID, presetGUID);
    }
    catch (std::exception& e)
    {
        DBGMSG(dbgERROR, L"%s: creating NvEncoder CreateDefaultEncoderParams threw exception : %s ", WFUNCTION, e.what());
        hr = E_FAIL;
    }
    catch (...)
    {
        DBGMSG(dbgERROR, L"%s: creating NvEncoder CreateDefaultEncoderParams threw unhandled exception", WFUNCTION);
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr) && pEncodeSettingsParams)
    {
        if (pEncodeSettingsParams->bitrate)
        {
            pInitializeParams->encodeConfig->rcParams.maxBitRate = pEncodeSettingsParams->bitrate * 1000;
            pInitializeParams->encodeConfig->rcParams.averageBitRate = pEncodeSettingsParams->bitrate * 1000;
        }

        if (pInitializeParams->encodeConfig->profileGUID != profileGUID)
        {
            pInitializeParams->encodeConfig->profileGUID = profileGUID;
        }

        if (pEncodeSettingsParams->rateControl == eNVENC_EncParamsRateControl::NVENC_EncParams_RC_VBR)
        {
            if (bTwoPass)
            {
                pInitializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR_HQ;
            }
            else
            {
                pInitializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
            }
            if (!pEncodeSettingsParams->maxBitrate || pEncodeSettingsParams->maxBitrate < pEncodeSettingsParams->bitrate)
                pInitializeParams->encodeConfig->rcParams.maxBitRate = 2 * pInitializeParams->encodeConfig->rcParams.averageBitRate;
            else
                pInitializeParams->encodeConfig->rcParams.maxBitRate = pEncodeSettingsParams->maxBitrate * 1000;
        }
        else if (pEncodeSettingsParams->rateControl == eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CBR)
        {
            if (bTwoPass)
            {
                pInitializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_HQ;
            }
            else
            {
                pInitializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
            }
        }
        else if (pEncodeSettingsParams->rateControl == eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CONSTQUAL)
        {
            // for CQ setting to VBR and to a predefined high max bitrate of 40000
            pInitializeParams->encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
            pInitializeParams->encodeConfig->rcParams.maxBitRate = MAX_BITRATE_FOR_RATECQ * 1000;
        }
        else
        {
            DBGMSG(dbgERROR, L"%s - No matching rate control specified, going with Auto", WFUNCTION, hr);
        }

        //For rate controls modes other than Constant quality, enable min QP..otherwise QP level drops and the bitrate goes very high
        //There are hard-coded values based on what the encoder team has provided.
        if (pEncodeSettingsParams->rateControl != eNVENC_EncParamsRateControl::NVENC_EncParams_RC_CONSTQUAL)
        { 
            pInitializeParams->encodeConfig->rcParams.enableMinQP = 1;
            pInitializeParams->encodeConfig->rcParams.minQP.qpIntra = 12;
            pInitializeParams->encodeConfig->rcParams.minQP.qpInterP = 14;
            pInitializeParams->encodeConfig->rcParams.minQP.qpInterB = 16;
            pInitializeParams->encodeConfig->rcParams.enableMaxQP = 1;
            pInitializeParams->encodeConfig->rcParams.maxQP.qpIntra = 50;
            pInitializeParams->encodeConfig->rcParams.maxQP.qpInterP = 51;
            pInitializeParams->encodeConfig->rcParams.maxQP.qpInterB = 51;
        }

        //TODO: required for twitch, need to figure out how to differentiate
        pInitializeParams->encodeConfig->encodeCodecConfig.h264Config.idrPeriod = 60;

        if (pEncodeSettingsParams->keyFrameInterval > 0)
        {
            pInitializeParams->encodeConfig->gopLength = pEncodeSettingsParams->keyFrameInterval;
        }
        else
        {
            pInitializeParams->encodeConfig->gopLength = NVENC_INFINITE_GOPLENGTH;
        }
        pInitializeParams->encodeConfig->encodeCodecConfig.h264Config.idrPeriod = pInitializeParams->encodeConfig->gopLength;

        if (pEncodeSettingsParams->bFrames == 0 || pInitializeParams->encodeConfig->gopLength == NVENC_INFINITE_GOPLENGTH)
        {
            DBGMSG(dbgERROR, L"%s - No B frames specified", WFUNCTION);
            pInitializeParams->encodeConfig->frameIntervalP = 1;
        }
        else
        {
            DBGMSG(dbgERROR, L"%s - Number of B frames specified - %d", WFUNCTION, pEncodeSettingsParams->bFrames);
            pInitializeParams->encodeConfig->frameIntervalP = 1 + pEncodeSettingsParams->bFrames;
        }

        if (pEncodeSettingsParams->frameRateNum != 0 && pEncodeSettingsParams->frameRateDen != 0)
        {
            pInitializeParams->frameRateNum = pEncodeSettingsParams->frameRateNum;
            pInitializeParams->frameRateDen = pEncodeSettingsParams->frameRateDen;
        }

        //lookahead not enabled for NVENC_EncParams_Preset_Fastest preset
        if (pEncodeSettingsParams->preset != eNVENC_EncParamsPreset::NVENC_EncParams_Preset_Fastest)
        {
           // check for lookahead and enable only when caps support it
            if (pEncodeObj->GetCapabilityValue(codecGUID, NV_ENC_CAPS_SUPPORT_LOOKAHEAD))
            {
                pInitializeParams->encodeConfig->rcParams.enableLookahead = pEncodeSettingsParams->advParams.enableLookAhead;
                if (pInitializeParams->encodeConfig->rcParams.enableLookahead)
                {
                    DBGMSG(dbgINFO, L"%s: NV_ENC_CAPS_SUPPORT_LOOKAHEAD caps supported and enabling lookahead", WFUNCTION);
                    pInitializeParams->encodeConfig->rcParams.lookaheadDepth = static_cast<uint16_t>(pEncodeSettingsParams->advParams.loopAheadDepth);
                }
            }
        }

        // check for temporal AQ and enable only when caps support it
        if (pEncodeObj->GetCapabilityValue(codecGUID, NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ))
        {
            if (pEncodeSettingsParams->advParams.enablePSY_AQ)
            {
                DBGMSG(dbgINFO, L"%s: NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ caps supported and enabling AQ", WFUNCTION);
                pInitializeParams->encodeConfig->rcParams.enableAQ = true;
                pInitializeParams->encodeConfig->rcParams.enableTemporalAQ = true;
            }
        }
        // check for b frame as reference caps and use it if supported
        if (pEncodeObj->GetCapabilityValue(codecGUID, NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE ))
        {
            DBGMSG(dbgINFO, L"%s: NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE caps supported", WFUNCTION);
            pInitializeParams->encodeConfig->encodeCodecConfig.h264Config.useBFramesAsRef = NV_ENC_BFRAME_REF_MODE_MIDDLE;
        }
    }
    return hr;
} //initializeEncodeConfig