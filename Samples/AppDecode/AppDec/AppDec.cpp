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

#include <iostream>
#include <algorithm>
#include <thread>
#include <cuda.h>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();


void ConvertToPlanar(uint8_t *pHostFrame, int nWidth, int nHeight, int nBitDepth) {
    if (nBitDepth == 8) {
        // nv12->iyuv
        YuvConverter<uint8_t> converter8(nWidth, nHeight);
        converter8.UVInterleavedToPlanar(pHostFrame);
    } else {
        // p016->yuv420p16
        YuvConverter<uint16_t> converter16(nWidth, nHeight);
        converter16.UVInterleavedToPlanar((uint16_t *)pHostFrame);
    }
}

void DecodeMediaFile(CUcontext cuContext, const char *szInFilePath, const char *szOutFilePath, bool bOutPlanar,
    const Rect &cropRect, const Dim &resizeDim)
{
    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    FFmpegDemuxer demuxer(szInFilePath);
    NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), false, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), NULL, false, false, &cropRect, &resizeDim);

    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, **ppFrame;
    do {
        demuxer.Demux(&pVideo, &nVideoBytes);
        dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
        if (!nFrame && nFrameReturned)
            LOG(INFO) << dec.GetVideoInfo();

        for (int i = 0; i < nFrameReturned; i++) {
            if (bOutPlanar) {
                ConvertToPlanar(ppFrame[i], dec.GetWidth(), dec.GetHeight(), dec.GetBitDepth());
            }
            fpOut.write(reinterpret_cast<char*>(ppFrame[i]), dec.GetFrameSize());
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);

    std::cout << "Total frame decoded: " << nFrame << std::endl
            << "Saved in file " << szOutFilePath << " in "
            << (dec.GetBitDepth() == 8 ? (bOutPlanar ? "iyuv" : "nv12") : (bOutPlanar ? "yuv420p16" : "p016"))
            << " format" << std::endl;
    fpOut.close();
}

void ShowDecoderCapability() {
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    std::cout << "Decoder Capability" << std::endl << std::endl;
    const char *aszCodecName[] = {"JPEG", "MPEG1", "MPEG2", "MPEG4", "H264", "HEVC", "HEVC", "HEVC", "VC1", "VP8", "VP9", "VP9", "VP9"};
    cudaVideoCodec aeCodec[] = { cudaVideoCodec_JPEG, cudaVideoCodec_MPEG1, cudaVideoCodec_MPEG2, cudaVideoCodec_MPEG4,
        cudaVideoCodec_H264, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_HEVC, cudaVideoCodec_VC1,
        cudaVideoCodec_VP8, cudaVideoCodec_VP9, cudaVideoCodec_VP9, cudaVideoCodec_VP9 };
    int anBitDepthMinus8[] = {0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 2, 4};
    for (int iGpu = 0; iGpu < nGpu; iGpu++) {
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        std::cout << "GPU " << iGpu << " - " << szDeviceName << std::endl << std::endl;
        for (int i = 0; i < sizeof(aeCodec) / sizeof(aeCodec[0]); i++) {
            CUVIDDECODECAPS decodeCaps = {};
            decodeCaps.eCodecType = aeCodec[i];
            decodeCaps.eChromaFormat = cudaVideoChromaFormat_420;
            decodeCaps.nBitDepthMinus8 = anBitDepthMinus8[i];

            cuvidGetDecoderCaps(&decodeCaps);
            std::cout << "Codec" << "  " << aszCodecName[i] << '\t' <<
                "BitDepth" << "  " << decodeCaps.nBitDepthMinus8 + 8 << '\t' <<
                "Supported" << "  " << (int)decodeCaps.bIsSupported << '\t' <<
                "MaxWidth" << "  " << decodeCaps.nMaxWidth << '\t' <<
                "MaxHeight" << "  " << decodeCaps.nMaxHeight << '\t' <<
                "MaxMBCount" << "  " << decodeCaps.nMaxMBCount << '\t' <<
                "MinWidth" << "  " << decodeCaps.nMinWidth << '\t' <<
                "MinHeight" << "  " << decodeCaps.nMinHeight << std::endl;
        }

        std::cout << std::endl;

        ck(cuCtxDestroy(cuContext));
    }
}

void ShowHelpAndExit(const char *szBadOption = NULL)
{
    bool bThrowError = false;
    std::ostringstream oss;
    if (szBadOption) 
    {
        bThrowError = true;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    oss << "Options:" << std::endl
        << "-i             Input file path" << std::endl
        << "-o             Output file path" << std::endl
        << "-outplanar     Convert output to planar format" << std::endl
        << "-gpu           Ordinal of GPU to use" << std::endl
        << "-crop l,t,r,b  Crop rectangle in left,top,right,bottom (ignored for case 0)" << std::endl
        << "-resize WxH    Resize to dimension W times H (ignored for case 0)" << std::endl
        ;
    oss << std::endl;
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        ShowDecoderCapability();
        exit(0);
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, char *szOutputFileName,
    bool &bOutPlanar, int &iGpu, Rect &cropRect, Dim &resizeDim)
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
        if (!_stricmp(argv[i], "-outplanar")) {
            bOutPlanar = true;
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-crop")) {
            if (++i == argc || 4 != sscanf(
                    argv[i], "%d,%d,%d,%d",
                    &cropRect.l, &cropRect.t, &cropRect.r, &cropRect.b)) {
                ShowHelpAndExit("-crop");
            }
            if ((cropRect.r - cropRect.l) % 2 == 1 || (cropRect.b - cropRect.t) % 2 == 1) {
                std::cout << "Cropping rect must have width and height of even numbers" << std::endl;
                exit(1);
            }
            continue;
        }
        if (!_stricmp(argv[i], "-resize")) {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &resizeDim.w, &resizeDim.h)) {
                ShowHelpAndExit("-resize");
            }
            if (resizeDim.w % 2 == 1 || resizeDim.h % 2 == 1) {
                std::cout << "Resizing rect must have width and height of even numbers" << std::endl;
                exit(1);
            }
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
}

/**
*  This sample application illustrates the demuxing and decoding of media file with
*  resize and crop of the output image. The application supports both planar (YUV420P and YUV420P16)
*  and non-planar (NV12 and P016) output formats.
*/

int main(int argc, char **argv) 
{
    char szInFilePath[256] = "", szOutFilePath[256] = "";
    bool bOutPlanar = false;
    int iGpu = 0;
    Rect cropRect = {};
    Dim resizeDim = {};
    try
    {
        ParseCommandLine(argc, argv, szInFilePath, szOutFilePath, bOutPlanar, iGpu, cropRect, resizeDim);
        CheckInputFile(szInFilePath);

        if (!*szOutFilePath) {
            sprintf(szOutFilePath, bOutPlanar ? "out.planar" : "out.native");
        }

        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu) {
            std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
            return 1;
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        std::cout << "GPU in use: " << szDeviceName << std::endl;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        std::cout << "Decode with demuxing." << std::endl;
        DecodeMediaFile(cuContext, szInFilePath, szOutFilePath, bOutPlanar, cropRect, resizeDim);
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }

    return 0;
}
