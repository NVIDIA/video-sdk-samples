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
#include <algorithm>
#include <memory>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/FFmpegDemuxer.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void GetImage(CUdeviceptr dpSrc, uint8_t *pDst, int nWidth, int nHeight)
{
    CUDA_MEMCPY2D m = { 0 };
    m.WidthInBytes = nWidth;
    m.Height = nHeight;
    m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    m.srcDevice = (CUdeviceptr)dpSrc;
    m.srcPitch = m.WidthInBytes;
    m.dstMemoryType = CU_MEMORYTYPE_HOST;
    m.dstDevice = (CUdeviceptr)(m.dstHost = pDst);
    m.dstPitch = m.WidthInBytes;
    cuMemcpy2D(&m);
}

void ShowHelpAndExit(const char *szBadOption = NULL)
{
    std::ostringstream oss;
    bool bThrowError = false;
    if (szBadOption)
    {
        bThrowError = true;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    oss << "Options:" << std::endl
        << "-i           Input file path" << std::endl
        << "-o           Output file path" << std::endl
        << "-of          Output format: native bgrp bgra bgra64" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        ;
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

enum OutputFormat 
{
    native = 0, bgrp, bgra, bgra64
};

std::vector<std::string> vstrOutputFormatName = 
{
    "native", "bgrp", "bgra", "bgra64"
};

void ParseCommandLine(int argc, char *argv[], char *szInputFileName,
    OutputFormat &eOutputFormat, char *szOutputFileName, int &iGpu)
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++) {
        if (!_stricmp(argv[i], "-h")) {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit("-i");
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o")) {
            if (++i == argc) {
                ShowHelpAndExit("-o");
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-of")) {
            if (++i == argc) {
                ShowHelpAndExit("-of");
            }
            auto it = find(vstrOutputFormatName.begin(), vstrOutputFormatName.end(), argv[i]);
            if (it == vstrOutputFormatName.end()) {
                ShowHelpAndExit("-of");
            }
            eOutputFormat = (OutputFormat)(it - vstrOutputFormatName.begin());
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
}

/**
*  This sample application illustrates the decoding of a media file in a desired color format.
*  The application supports native (nv12 or p016), bgra, bgrp and bgra64 output formats.
*/

int main(int argc, char **argv)
{
    char szInFilePath[256] = "", szOutFilePath[256] = "";
    OutputFormat eOutputFormat = native;
    int iGpu = 0;
    bool bReturn = 1;
    CUdeviceptr pTmpImage = 0;

    try
    {
        ParseCommandLine(argc, argv, szInFilePath, eOutputFormat, szOutFilePath, iGpu);
        CheckInputFile(szInFilePath);

        if (!*szOutFilePath)
        {
            sprintf(szOutFilePath, "out.%s", vstrOutputFormatName[eOutputFormat].c_str());
        }

        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::ostringstream err;
            err << "Unable to open output file: " << szOutFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }

        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu)
        {
            std::ostringstream err;
            err << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]";
            throw std::invalid_argument(err.str());
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        LOG(INFO) << "GPU in use: " << szDeviceName;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        FFmpegDemuxer demuxer(szInFilePath);
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
        int nWidth = demuxer.GetWidth(), nHeight = demuxer.GetHeight();
        int anSize[] = { 0, 3, 4, 8 };
        int nFrameSize = eOutputFormat == native ? demuxer.GetFrameSize() : nWidth * nHeight * anSize[eOutputFormat];
        std::unique_ptr<uint8_t[]> pImage(new uint8_t[nFrameSize]);

        int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
        uint8_t *pVideo = NULL;
        uint8_t **ppFrame;

        cuMemAlloc(&pTmpImage, nWidth * nHeight * anSize[eOutputFormat]);

        do {
            demuxer.Demux(&pVideo, &nVideoBytes);
            dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
            if (!nFrame && nFrameReturned)
                LOG(INFO) << dec.GetVideoInfo();

            for (int i = 0; i < nFrameReturned; i++)
            {
                if (dec.GetBitDepth() == 8) {
                    switch (eOutputFormat) {
                    case native:
                        GetImage((CUdeviceptr)ppFrame[i], reinterpret_cast<uint8_t*>(pImage.get()), dec.GetWidth(), 3 * dec.GetHeight() / 2);
                        break;
                    case bgrp:
                        Nv12ToBgrPlanar((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t*)pTmpImage, dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), dec.GetWidth(), 3 * dec.GetHeight());
                        break;
                    case bgra:
                        Nv12ToBgra32((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t*)pTmpImage, 4 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), 4 * dec.GetWidth(), dec.GetHeight());
                        break;
                    case bgra64:
                        Nv12ToBgra64((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t*)pTmpImage, 8 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), 8 * dec.GetWidth(), dec.GetHeight());
                        break;
                    }
                }
                else
                {
                    switch (eOutputFormat) {
                    case native:
                        GetImage((CUdeviceptr)ppFrame[i], reinterpret_cast<uint8_t*>(pImage.get()), 2 * dec.GetWidth(), 3 * dec.GetHeight() / 2);
                        break;
                    case bgrp:
                        P016ToBgrPlanar((uint8_t *)ppFrame[i], 2 * dec.GetWidth(), (uint8_t*)pTmpImage, dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), dec.GetWidth(), 3 * dec.GetHeight());
                        break;
                    case bgra:
                        P016ToBgra32((uint8_t *)ppFrame[i], 2 * dec.GetWidth(), (uint8_t*)pTmpImage, 4 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), 4 * dec.GetWidth(), dec.GetHeight());
                        break;
                    case bgra64:
                        P016ToBgra64((uint8_t *)ppFrame[i], 2 * dec.GetWidth(), (uint8_t*)pTmpImage, 8 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), 8 * dec.GetWidth(), dec.GetHeight());
                        break;
                    }
                }

                fpOut.write(reinterpret_cast<char*>(pImage.get()), nFrameSize);
            }
            nFrame += nFrameReturned;
        } while (nVideoBytes);

        if (pTmpImage) {
            cuMemFree(pTmpImage);
        }

        std::cout << "Total frame decoded: " << nFrame << std::endl << "Saved in file " << szOutFilePath << std::endl;
        fpOut.close();
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
