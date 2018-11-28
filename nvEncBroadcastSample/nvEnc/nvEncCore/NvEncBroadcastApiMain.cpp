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

#include "nvEncBroadcastUtils.h"
#include "nvEncBroadcastEncodeApi.h"
#include "nvEncBroadcastObject.h"

// global debug level
DebugLevel  g_dbgLevel = dbgINFO;
DebugLevel  getGlobalDbgLevel() { return g_dbgLevel; }

namespace nvEncBroadcastApi
{
    NVENCBROADCAST_EXPORT eNVENC_RetCode __stdcall NVENC_EncodeInitialize(NVENC_EncodeCreateParams* pCreateParams, INVENC_EncodeApiObj** ppEncodeApiImpl)
    {
        DBGMSG(dbgERROR, L"%s - NVENC_EncodeInitialize called", WFUNCTION);

        eNVENC_RetCode retCode = API_SUCCESS;
        INVENC_EncodeApiObj* pEncodeObj = nullptr;

        if (!pCreateParams)
        {
            DBGMSG(dbgERROR, L"%s: NULL Create Params ptr recieved", WFUNCTION);
            return API_ERR_INVALID_PARAMETERS;
        }

        if (pCreateParams->version > NVENC_ENCODEAPI_VER)
        {
            DBGMSG(dbgERROR, L"%s: Invalid %x version number compared top supported %x version number", WFUNCTION, pCreateParams->version, NVENC_ENCODEAPI_VER);
            return API_ERR_INVALID_PARAMETERS;
        }

        if (!ppEncodeApiImpl && (!pCreateParams->createFlags & NVENC_EncodeCanCreate_Flag))
        {
            DBGMSG(dbgERROR, L"%s: NULL INVENC_EncodeApiObj ptr recieved and no NVENC_EncodeCanCreate_Flag specified", WFUNCTION);
            return API_ERR_INVALID_PARAMETERS;
        }

        if (ppEncodeApiImpl)
        {
            UINT32 uiVersion = pCreateParams->version;
            if (!uiVersion)
            {
                uiVersion = NVENC_ENCODEAPI_VER;
            }

            if (uiVersion == NVENC_ENCODEAPI_VER1)
            {
                pEncodeObj = nvEncBroadcastObj::CreateInstance(pCreateParams);
                if (!pEncodeObj)
                {
                    *ppEncodeApiImpl = nullptr;
                    retCode = API_ERR_OUT_OF_MEMORY;
                }
            }
            else
            {
                *ppEncodeApiImpl = nullptr;
                retCode = API_ERR_INVALID_VER;
            }

            if (SUCCEEDED_NVENC_BROADCASTAPI(retCode))
            {
                // now set the interface back on the 
                *ppEncodeApiImpl = pEncodeObj;
            }
        }
        return retCode;
    }//NVENC_EncodeInitialize
} //namespace