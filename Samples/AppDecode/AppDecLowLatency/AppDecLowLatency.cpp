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

#include <vector>
#include <cuda.h>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../Common/AppDecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

/**
*  This sample application demonstrates low latency decoding feature. This feature helps to get
*  output frame as soon as it is decoded without any delay. The feature will work for streams having
*  I and P frames only.
*/

int main(int argc, char *argv[]) 
{
    char szInFilePath[256] = "", szOutFilePath[256] = "out.nv12";
    int iGpu = 0;
    bool bVerbose = false;
    try
    {
        ParseCommandLine(argc, argv, szInFilePath, szOutFilePath, iGpu, &bVerbose);
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
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        FFmpegDemuxer demuxer(szInFilePath);
        /* Here set bLowLatency=true in the constructor.
           Please don't use this flag except for low latency, it is harder to get 100% utilization of
           hardware decoder with this flag set. */
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), false, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), NULL, true);

        int nFrame = 0;
        uint8_t *pVideo = NULL;
        int nVideoBytes = 0;
        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::ostringstream err;
            err << "Unable to open output file: " << szOutFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }

        int n = 0;
        bool bOneInOneOut = true;
        uint8_t **ppFrame;
        int64_t *pTimestamp;
        int nFrameReturned = 0;
        do {
            demuxer.Demux(&pVideo, &nVideoBytes);
            // Set flag CUVID_PKT_ENDOFPICTURE to signal that a complete packet has been sent to decode
            dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned, CUVID_PKT_ENDOFPICTURE, &pTimestamp, n++);
            if (!nFrame && nFrameReturned)
                LOG(INFO) << dec.GetVideoInfo();

            nFrame += nFrameReturned;
            // For a stream without B-frames, "one in and one out" is expected, and nFrameReturned should be always 1 for each input packet
            if (bVerbose)
            {
                std::cout << "Decode: nVideoBytes=" << nVideoBytes << ", nFrameReturned=" << nFrameReturned << ", total=" << nFrame << std::endl;
            }
            if (nVideoBytes && nFrameReturned != 1)
            {
                bOneInOneOut = false;
            }
            for (int i = 0; i < nFrameReturned; i++) 
            {
                if (bVerbose)
                {
                    std::cout << "Timestamp: " << pTimestamp[i] << std::endl;
                }
                fpOut.write(reinterpret_cast<char*>(ppFrame[i]), dec.GetFrameSize());
            }
        } while (nVideoBytes);

        fpOut.close();
        std::cout << "One packet in and one frame out: " << (bOneInOneOut ? "true" : "false") << std::endl;
    }
    catch(const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
