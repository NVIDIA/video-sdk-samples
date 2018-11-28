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
#include "FramePresenterD3D9.h"
#include "FramePresenterD3D11.h"
#include "../Common/AppDecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

template<class FramePresenterType, typename = std::enable_if<std::is_base_of<FramePresenterD3D, FramePresenterType>::value>>
int NvDecD3D(CUcontext cuContext, char *szInFilePath)
{
    FFmpegDemuxer demuxer(szInFilePath);
    NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
    FramePresenterType presenter(cuContext, demuxer.GetWidth(), demuxer.GetHeight());
    CUdeviceptr dpFrame = 0;
    ck(cuMemAlloc(&dpFrame, demuxer.GetWidth() * demuxer.GetHeight() * 4));
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, **ppFrame;

    do
    {
        demuxer.Demux(&pVideo, &nVideoBytes);
        dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
        if (!nFrame && nFrameReturned)
            LOG(INFO) << dec.GetVideoInfo();

        for (int i = 0; i < nFrameReturned; i++)
        {
            if (dec.GetBitDepth() == 8)
                Nv12ToBgra32((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t *)dpFrame, 4 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
            else
                P016ToBgra32((uint8_t *)ppFrame[i], 2 * dec.GetWidth(), (uint8_t *)dpFrame, 4 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
            presenter.PresentDeviceFrame((uint8_t *)dpFrame, demuxer.GetWidth() * 4);
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);
    ck(cuMemFree(dpFrame));
    std::cout << "Total frame decoded: " << nFrame << std::endl;
    return 0;
}

/**
*  This sample application illustrates the decoding of media file and display of decoded frames
*  in a window. This is done by CUDA interop with Direct3D.
*/

int main(int argc, char **argv) 
{
    char szInFilePath[256] = "";
    int iGpu = 0;
    int iD3d = 0;
    try
    {
        ParseCommandLine(argc, argv, szInFilePath, NULL, iGpu, NULL, &iD3d);
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

        switch (iD3d) {
        default:
        case 9:
            std::cout << "Display with D3D9." << std::endl;
            return NvDecD3D<FramePresenterD3D9>(cuContext, szInFilePath);
        case 11:
            std::cout << "Display with D3D11." << std::endl;
            return NvDecD3D<FramePresenterD3D11>(cuContext, szInFilePath);
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
