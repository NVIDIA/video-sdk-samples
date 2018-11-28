/*
* Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

#include <cuda.h>
#include <iostream>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "FramePresenterGL.h"
#include "../Common/AppDecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

int Decode(CUcontext cuContext, char *szInFilePath) {

    FFmpegDemuxer demuxer(szInFilePath);
    NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
    FramePresenterGL presenter(cuContext, demuxer.GetWidth(), demuxer.GetHeight());
    uint8_t *dpFrame = 0;
    int nPitch = 0;
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL;
    uint8_t **ppFrame;
    do {
        demuxer.Demux(&pVideo, &nVideoBytes);
        dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
        if (!nFrame && nFrameReturned)
            LOG(INFO) << dec.GetVideoInfo();

        for (int i = 0; i < nFrameReturned; i++) {
            presenter.GetDeviceFrameBuffer(&dpFrame, &nPitch);
            if (dec.GetBitDepth() == 8)
                Nv12ToBgra32((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t *)dpFrame, nPitch, dec.GetWidth(), dec.GetHeight());
            else
                P016ToBgra32((uint8_t *)ppFrame[i], 2 * dec.GetWidth(), (uint8_t *)dpFrame, nPitch, dec.GetWidth(), dec.GetHeight());
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);
    std::cout << "Total frame decoded: " << nFrame << std::endl;
    return 0;
}

/**
*  This sample application illustrates the decoding of media file and display of decoded frames
*  in a window. This is done by CUDA interop with OpenGL.
*/

int main(int argc, char **argv) 
{
    char szInFilePath[256] = "";
    int iGpu = 0;
    try
    {
        ParseCommandLine(argc, argv, szInFilePath, NULL, iGpu, NULL, NULL);
        CheckInputFile(szInFilePath);

        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu)
        {
            std::ostringstream err;
            err << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
            throw std::invalid_argument(err.str());
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        std::cout << "GPU in use: " << szDeviceName << std::endl;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, CU_CTX_SCHED_BLOCKING_SYNC, cuDevice));

        std::cout << "Decode with NvDecoder." << std::endl;
        Decode(cuContext, szInFilePath);
    }
    catch(const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
