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

#ifndef _NVENCBROADCAST_ENCODEAPI_H
#define _NVENCBROADCAST_ENCODEAPI_H

#include <stdint.h>
#include "nvEncBroadcastOsDefines.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace nvEncBroadcastApi
{
    // Max lengths for all strings passed..Technically MAX_LENGTH-1
    #ifndef MAX_LENGTH
    #define MAX_LENGTH     4096
    #endif
    //-------------------------------------------------------------------
    // enum eNVENC_RetCode
    //-------------------------------------------------------------------
    enum eNVENC_RetCode
    {
        API_SUCCESS = 0,
        API_ERR_GENERIC = -1,
        API_ERR_INVALID_VER = -2,
        API_ERR_UNINITIALIZED = -3,
        API_ERR_UNSUPPORTED = -4,
        API_ERR_INVALID_PARAMETERS = -5,
        API_ERR_INSUFFICIENT_BUFFER = -6,
        API_ERR_NO_IMPLEMENTATION = -7,
        API_ERR_ALREADY_CREATED = -8,
        API_ERR_NOTAVAILABLE = -9,
        API_ERR_OUT_OF_MEMORY = -10,
        API_ERR_XCODE_FAIL = -11,
        API_ERR_OUT_OF_SIZE = -12,
        //...
        API_PENDING = -256,
    };

    //-------------------------------------------------------------------
    // enum eNVENC_EventNotifyType
    //-------------------------------------------------------------------
    enum eNVENC_EventNotifyType
    {
        kEventNotifyType_Callback = 0x00000001,
        kEventNotifyType_WndHandle,
    };

    //-------------------------------------------------------------------
    // func pfnEventHandler
    //-------------------------------------------------------------------
    typedef void (CALLBACK * pfnEventHandler)(uint32_t eventType, uint32_t eventState);

    //-------------------------------------------------------------------
    // struct NVENC_EventNotify
    //-------------------------------------------------------------------
    typedef struct _NVENC_EventNotify
    {
        eNVENC_EventNotifyType notifyType;
        union
        {
            pfnEventHandler  onEventChangeNotify;         // IN parameter, will be invoked when a server state change is detected
    #ifdef WIN_OS
            struct
            {                                      
                HWND          hwnd;                       // Window Handle recieving the messages
                uint32_t      baseWindowUserMesaage;      // event message sent out will be one more than this. defined by EventItems
            }wndMessage;
    #endif
            uint32_t   reserved[8];
        };
    } NVENC_EventNotify;

    //-------------------------------------------------------------------
    // enum eNVENC_BufferType
    //-------------------------------------------------------------------
    enum eNVENC_BufferType
    {
        kBufferType_Sys = 0x00000001,
        kBufferType_Vid = 0x00000002,
        kBufferType_Max,
    };

    //-------------------------------------------------------------------
    // enum eNVENC_DxBufferType
    //-------------------------------------------------------------------
    enum eNVENC_DxBufferType
    {
        kDxBufferType_DX9 = 0x00000001,
        kDxBufferType_DX10 = 0x00000002,
        kDxBufferType_DX11 = 0x00000004,
        kDxBufferType_DX12 = 0x00000008,
        kDxBufferType_Max,
    };

    //-------------------------------------------------------------------
    // enum eNVENC_BufferFormat
    //-------------------------------------------------------------------
    enum eNVENC_BufferFormat
    {
        kBufferFormat_None       = 0,
        kBufferFormat_ARGB       = 1,              /**< Output Pixels in ARGB format*/
        kBufferFormat_NV12       = 2,              /**< Output Pixels in NV12 format*/
        kBufferFormat_ABGR       = 3,              /**< Output Pixels in ABGR format*/
        kBufferFormat_Max, 
    };

    //-------------------------------------------------------------------
    // enum eDXSurfaceShareKey = DX Surface sharing mutex keys
    //-------------------------------------------------------------------
    enum eDXSurfaceShareKey {
        DXSURFACE_CREATORACQUIRE_USERRELEASE_KEY = 0,
        DXSURFACE_CREATORRELEASE_USERACQUIRE_KEY = 1,
    };

    //-------------------------------------------------------------------
    // struct NVENC_BufferInfo
    //-------------------------------------------------------------------
    typedef struct _NVENC_BufferInfo
    {
        eNVENC_BufferType       bufferType;
        eNVENC_DxBufferType     dxBufferType;
        eNVENC_BufferFormat     bufferFormat;
        union
        {
            struct
            {                                      
                uint32_t      lineWidth;        /** Indicates the current width of the pixel buffer(padded width). */
                uint64_t      pixelBuffer;      
            }SysBuffer;
    #ifdef WIN_OS
            struct
            {                                      
                uint32_t      lineWidth;        /**< Indicates the current width of the pixel buffer(padded width). */
                HANDLE        bufferHandle;     
            }DxBuffer;
    #endif
            uint32_t   reserved[8];
        };
    } NVENC_BufferInfo;

    //-------------------------------------------------------------------
    // struct NVENC_EncodeInfo
    //-------------------------------------------------------------------
    typedef struct _NVENC_EncodeInfo
    {
        NVENC_BufferInfo  bufferInfo;
        int64_t            pts;
        uint32_t           reserved[4];          /**< [in] Resereved, should be set to 0. */
    } NVENC_EncodeInfo;

    //-------------------------------------------------------------------
    // struct NVENC_EncodeInitParams
    //-------------------------------------------------------------------
    typedef struct _NVENC_EncodeInitParams
    {
        uint32_t   width;                    /**< [in] Indicates the current width of the video frame*/
        uint32_t   height;                   /**< [in] Indicates the current height of the video frame */
        eNVENC_BufferFormat bufferFormat;  /**< [in] Indicates the buffer format of the video frame. */
    } NVENC_EncodeInitParams;

    //-------------------------------------------------------------------
    // Rate Control Modes
    //-------------------------------------------------------------------
    enum class eNVENC_EncParamsRateControl
    {
        NVENC_EncParams_RC_VBR             = 0x0,       /**< Variable bitrate mode */
        NVENC_EncParams_RC_CBR             = 0x1,       /**< Constant bitrate mode */
        NVENC_EncParams_RC_CONSTQUAL       = 0x2,       /**< Constant quality mode */
        NVENC_EncParams_RC_Max
    };

    //-------------------------------------------------------------------
    // H264 profile
    //-------------------------------------------------------------------
    enum class eNVENC_EncParamsProfile
    {
        NVENC_EncParams_Profile_Auto       = 0x0,
        NVENC_EncParams_Profile_Main       = 0x0,
        NVENC_EncParams_Profile_Baseline   = 0x1,
        NVENC_EncParams_Profile_High       = 0x2,
        NVENC_EncParams_Profile_Max
    };

    //-------------------------------------------------------------------
    // encoding presets
    //-------------------------------------------------------------------
    enum class eNVENC_EncParamsPreset
    {
        NVENC_EncParams_Preset_Medium     = 0x0,
        NVENC_EncParams_Preset_Default    = 0x0,
        NVENC_EncParams_Preset_Slow       = 0x1,
        NVENC_EncParams_Preset_Fast       = 0x2,
        NVENC_EncParams_Preset_Fastest    = 0x3,
        NVENC_EncParams_Preset_Max,
    };

    enum class eNVENC_EncParamsLevel
    {
         NVENC_EncParams_Level_Auto               = 0,
         NVENC_EncParams_Level_H264_1             = 10,
         NVENC_EncParams_Level_H264_1b            = 9,
         NVENC_EncParams_Level_H264_11            = 11,
         NVENC_EncParams_Level_H264_12            = 12,
         NVENC_EncParams_Level_H264_13            = 13,
         NVENC_EncParams_Level_H264_2             = 20,
         NVENC_EncParams_Level_H264_21            = 21,
         NVENC_EncParams_Level_H264_22            = 22,
         NVENC_EncParams_Level_H264_3             = 30,
         NVENC_EncParams_Level_H264_31            = 31,
         NVENC_EncParams_Level_H264_32            = 32,
         NVENC_EncParams_Level_H264_4             = 40,
         NVENC_EncParams_Level_H264_41            = 41,
         NVENC_EncParams_Level_H264_42            = 42,
         NVENC_EncParams_Level_H264_5             = 50,
         NVENC_EncParams_Level_H264_51            = 51,
         NVENC_EncParams_Level_H264_52            = 52,

         NVENC_EncParams_Level_HEVC_1             = 30,
         NVENC_EncParams_Level_HEVC_2             = 60,
         NVENC_EncParams_Level_HEVC_21            = 63,
         NVENC_EncParams_Level_HEVC_3             = 90,
         NVENC_EncParams_Level_HEVC_31            = 93,
         NVENC_EncParams_Level_HEVC_4             = 120,
         NVENC_EncParams_Level_HEVC_41            = 123,
         NVENC_EncParams_Level_HEVC_5             = 150,
         NVENC_EncParams_Level_HEVC_51            = 153,
         NVENC_EncParams_Level_HEVC_52            = 156,
         NVENC_EncParams_Level_HEVC_6             = 180,
         NVENC_EncParams_Level_HEVC_61            = 183,
         NVENC_EncParams_Level_HEVC_62            = 186,
    };

    //-------------------------------------------------------------------
    // struct NVENC_AdvancedEncodeParams
    //-------------------------------------------------------------------
    typedef union _NVENC_AdvancedEncodeParams{
        struct {
            uint32_t     enablePSY_AQ         :1;
            uint32_t     enableLookAhead      :1;
            uint32_t     reservedBitFields    :30;      
            uint32_t     loopAheadDepth;
        };
        uint32_t         reserved[4];
    } NVENC_AdvancedEncodeParams;

    //-------------------------------------------------------------------
    // struct NVENC_EncodeSettingsParams
    //-------------------------------------------------------------------
    typedef struct _NVENC_EncodeSettingsParams
    {
        uint32_t                      bitrate;                     // Bit rate in multiple of 1000's. For example 4Mbps will be 4000
        uint32_t                      maxBitrate;                  // Same as above 
        uint32_t                      frameRateNum;                // Frame rate numerator and denominator denoting frame rate...as an example, for 30fps, Num can be 30000, den can be 1000. For 29.97, den can be 1001.
        uint32_t                      frameRateDen;
        uint32_t                      keyFrameInterval;            // Key frame interval in secs.
        eNVENC_EncParamsRateControl   rateControl;                 // rate control values as defined above.
        eNVENC_EncParamsProfile       encProfile;                  // enc profile
        eNVENC_EncParamsPreset        preset;                      // enc presets depending upon end user scenario
        eNVENC_EncParamsLevel         level;                       // enc levels as defined by the spec
        uint32_t                      bFrames;                     // number of b frames following I, P.
        uint32_t                      constantQuality;             // constant quality knob if rate control mode chosen as CQ.
        NVENC_AdvancedEncodeParams    advParams;                   // advanced params.
    } NVENC_EncodeSettingsParams;

    //-------------------------------------------------------------------
    // Time stamp to be retrieved from the bitstream
    //-------------------------------------------------------------------
    enum eNVENC_TimeStampType
    {
        NVENC_TimeStamp_Pts                = 0x0,       /** presentation timestamp */
        NVENC_TimeStamp_Dts                = 0x1,       /** decoding time stamp */
        NVENC_TimeStamp_Max                = 0x2,
    };

    //-------------------------------------------------------------------
    // INVENC_EncodeBitstreamBuffer interface defintion
    // abstract class
    //-------------------------------------------------------------------
    class NVENCBROADCAST_NOVTABLE INVENC_EncodeBitstreamBuffer
    {
    public:
        //is bit stream buffer correctly initialized
        virtual bool NVENCBROADCAST_Func isInitalized() const;

        //release the buffer
        virtual eNVENC_RetCode NVENCBROADCAST_Func release() = 0;

        //recycle the buffer for reuse
        virtual eNVENC_RetCode NVENCBROADCAST_Func recycle() = 0;

        //is current packet a keyframe
        virtual bool NVENCBROADCAST_Func isKeyFrame() const = 0;

        //get the timestamp for the current bitstream
        virtual uint64_t NVENCBROADCAST_Func getPts(eNVENC_TimeStampType tsType) const = 0;

        //get the actual bitstream associated with the buffer
        virtual uint8_t* NVENCBROADCAST_Func getBitStreamBuffer(uint32_t* size) const = 0;

        virtual ~INVENC_EncodeBitstreamBuffer() = 0 {
        }

    protected:
        INVENC_EncodeBitstreamBuffer() {
        }
    };

    //-------------------------------------------------------------------
    // INVENC_EncodeApiObj interface defintion
    // abstract class
    // Version information to be associated with the class
    //-------------------------------------------------------------------
    #define NVENC_ENCODEAPI_VER1     1
    class NVENCBROADCAST_NOVTABLE INVENC_EncodeApiObj
    {
    public:
        //create or instantiate the feature
        virtual eNVENC_RetCode NVENCBROADCAST_Func initialize(NVENC_EncodeInitParams* pEncodeInitParams, NVENC_EncodeSettingsParams* pEncodeSettingsParams) = 0;

        virtual eNVENC_RetCode NVENCBROADCAST_Func createBitstreamBuffer(INVENC_EncodeBitstreamBuffer** ppBuffer) = 0;

        //encode the provided frame
        virtual eNVENC_RetCode NVENCBROADCAST_Func encode(NVENC_EncodeInfo* pEncodeInfo, INVENC_EncodeBitstreamBuffer* ppBuffer) = 0;

        //encode the provided frame
        virtual eNVENC_RetCode NVENCBROADCAST_Func getSequenceParams(INVENC_EncodeBitstreamBuffer* ppBuffer) = 0;

        //finalize the encoder operation
        virtual eNVENC_RetCode NVENCBROADCAST_Func finalize(INVENC_EncodeBitstreamBuffer* pBuffer) = 0;

        //is encode initialized
        virtual bool NVENCBROADCAST_Func isInitalized() const = 0;

        // release INVENC_EncodeApiObj object
        virtual eNVENC_RetCode NVENCBROADCAST_Func releaseObject() = 0;

        virtual ~INVENC_EncodeApiObj() = 0 {
        }

    protected:
        INVENC_EncodeApiObj() {
        }
    };
    #define NVENC_ENCODEAPI_VER           NVENC_ENCODEAPI_VER1


    //-------------------------------------------------------------------
    // Creation Flags
    //-------------------------------------------------------------------
    enum eNVENC_EncodeCreateFlags
    {
        NVENC_EncodeCanCreate_Flag            = 0x0,              // Verify if encoder is supported on the platform. When this flag is specified, ppEncodeApiImpl can be NULL
        NVENC_EncodeFlag_Max
    };

    //-------------------------------------------------------------------
    // NVENC_EncodeCreateParams struct
    // struct used by app to interface with the API exported function NVENC_EncodeInitialize
    //-------------------------------------------------------------------
    typedef struct _NVENC_EncodeCreateParams
    {
        uint32_t                         size;                    // IN parameter, size of the struct
        uint32_t                         version;                 // IN parameter, Version of the API
        eNVENC_EncodeCreateFlags         createFlags;
        NVENC_EventNotify                eventNotify;             // IN parameter, struct to describe how notification events are to be sent. Currently not suupported but can be added
    } NVENC_EncodeCreateParams;

    //-------------------------------------------------------------------
    // NVENC_EncodeInitialize function declaration
    // NVENC_EncodeInitialize: The exported function from the dll
    //-------------------------------------------------------------------
    typedef eNVENC_RetCode (__stdcall *pfnNVENC_EncodeInitialize)(NVENC_EncodeCreateParams* pCreateParams, INVENC_EncodeApiObj** ppEncodeApiImpl);
    NVENCBROADCAST_EXPORT  eNVENC_RetCode __stdcall NVENC_EncodeInitialize(/* [in, out]*/ NVENC_EncodeCreateParams* pCreateParams, INVENC_EncodeApiObj** ppEncodeApiImpl);

} // namespace nvEncBroadcastApi

#ifdef __cplusplus
};
#endif

#endif // _NVENCBROADCAST_ENCODEAPI_H
