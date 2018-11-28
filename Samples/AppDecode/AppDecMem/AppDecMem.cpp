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
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../Common/AppDecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

class FileDataProvider : public FFmpegDemuxer::DataProvider {
public:
    FileDataProvider(const char *szInFilePath) {
        fpIn.open(szInFilePath, std::ifstream::in | std::ifstream::binary);
        if (!fpIn)
        {
            std::cout << "Unable to open input file: " << szInFilePath << std::endl;
            return;
        }
    }
    ~FileDataProvider() {
        fpIn.close();
    }
    // Fill in the buffer owned by the demuxer/decoder
    int GetData(uint8_t *pBuf, int nBuf) {
        // We read a file for this example. You may get your data from network or somewhere else
        return (int)fpIn.read(reinterpret_cast<char*>(pBuf), nBuf).gcount();
    }

private:
    std::ifstream fpIn;
};

/**
*  This sample application illustrates shows how to demux and decode media content from
*  memory buffer.
*/

int main(int argc, char *argv[])
{
    char szInFilePath[256] = "", szOutFilePath[256] = "out.nv12";
    int iGpu = 0;
    try
    {
        ParseCommandLine(argc, argv, szInFilePath, szOutFilePath, iGpu);
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

        FileDataProvider dp(szInFilePath);
        /* Instead of passing in a media file path, here we pass in a DataProvider, which reads from the file.
           Note that the data is passed into the demuxer chunk-by-chunk sequentially. If the meta data is at the end of the file
           (as for MP4) and the buffer isn't large enough to hold the whole file, the file may never get demuxed.*/
        FFmpegDemuxer demuxer(&dp);
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), false, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));

        int nFrame = 0;
        uint8_t *pVideo = NULL;
        int nVideoBytes = 0;
        uint8_t **ppFrame;
        int nFrameReturned = 0;
        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::ostringstream err;
            err << "Unable to open output file: " << szOutFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }
        do
        {
            demuxer.Demux(&pVideo, &nVideoBytes);
            dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
            if (!nFrame && nFrameReturned)
                LOG(INFO) << dec.GetVideoInfo();

            nFrame += nFrameReturned;
            for (int i = 0; i < nFrameReturned; i++) {
                fpOut.write(reinterpret_cast<char*>(ppFrame[i]), dec.GetFrameSize());
            }
        } while (nVideoBytes);
        fpOut.close();
        std::cout << "Total frame decoded: " << nFrame << std::endl << "Saved in file " << szOutFilePath;
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
