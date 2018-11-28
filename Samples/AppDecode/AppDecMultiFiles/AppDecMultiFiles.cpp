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
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

typedef struct
{
    char inFile[256];
    char outFile[256];
    Dim resizeDim;
    Rect cropRect;
    bool outplanar;
} FILEINFO;

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

void getMaxWidthandMaxHeight(int &maxWidth, int &maxHeight, cudaVideoCodec aeCodec, int anBitDepthMinus8)
{
    CUVIDDECODECAPS decodeCaps = {};
    decodeCaps.eCodecType = aeCodec;
    decodeCaps.eChromaFormat = cudaVideoChromaFormat_420;
    decodeCaps.nBitDepthMinus8 = anBitDepthMinus8;

    cuvidGetDecoderCaps(&decodeCaps);

    maxWidth = decodeCaps.nMaxWidth;
    maxHeight = decodeCaps.nMaxHeight;

}

void DecodeMediaFile(CUcontext cuContext, NvDecoder **pDec, FILEINFO fileData, int useReconfigure, int maxWidth = 0, int maxHeight = 0)
{
    std::ofstream fpOut(fileData.outFile, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << fileData.outFile << std::endl;
        throw std::invalid_argument(err.str());
    }

    FFmpegDemuxer demuxer(fileData.inFile);
    NvDecoder *dec = *pDec;

    if (useReconfigure)
    {
        if (dec == NULL)
        {
            if ((!maxWidth) || (!maxHeight))
            {
                // Get MaxWidth/MaxHeight for particular codec and bitdepth if not set in commandline
                getMaxWidthandMaxHeight(maxWidth, maxHeight, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), demuxer.GetBitDepth() - 8);
            }

            dec = new NvDecoder(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), false, FFmpeg2NvCodecId(demuxer.GetVideoCodec()),
                0, false, false, &fileData.cropRect, &fileData.resizeDim, maxWidth, maxHeight);
            *pDec = dec;
        }
        else
        {
            dec->setReconfigParams(&fileData.cropRect, &fileData.resizeDim);
        }

    }
    else
    {
        dec = new NvDecoder(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), false, FFmpeg2NvCodecId(demuxer.GetVideoCodec()),
            0, false, false, &fileData.cropRect, &fileData.resizeDim);
    }

    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, **ppFrame;
    do {
        demuxer.Demux(&pVideo, &nVideoBytes);
        dec->Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
        if (!nFrame && nFrameReturned)
            LOG(INFO) << dec->GetVideoInfo();

        for (int i = 0; i < nFrameReturned; i++) {
            if (fileData.outplanar) {
                ConvertToPlanar(ppFrame[i], dec->GetWidth(), dec->GetHeight(), dec->GetBitDepth());
            }
            fpOut.write(reinterpret_cast<char*>(ppFrame[i]), dec->GetFrameSize());
        }
        nFrame += nFrameReturned;
    } while (nVideoBytes);

    std::cout << "Total frame decoded: " << nFrame << std::endl
            << "Saved in file " << fileData.outFile << " in "
            << (dec->GetBitDepth() == 8 ? (fileData.outplanar ? "iyuv" : "nv12") : (fileData.outplanar ? "yuv420p16" : "p016"))
            << " format" << std::endl;
    if (!useReconfigure)
    {
        delete dec;
        dec = NULL;
    }
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
        << "-filelist  list.txt  (File which contains all files to be decoded in below format)" << std::endl
        << "    Example list.txt:" << std::endl
        << "    infile  input1.h264 (Input file path)" << std::endl
        << "    outfile out1.yuv    (Output file path)" << std::endl
        << "    outplanar 1         (Convert output to planar format)" << std::endl
        << "    resize WxH          (Resize to dimension Width x Height)" << std::endl
        << "    crop l,t,r,b        (Crop rectangle in left,top,right,bottom)" << std::endl
        << "    infile  input2.h264 " << std::endl
        << "    outfile out2.yuv    " << std::endl
        << "    ....." << std::endl
        << "    ....." << std::endl
        << "-gpu gpuId           (Ordinal of GPU to use)" << std::endl
        << "-usereconfigure flag (flag is true by default, set to 0 to disable reconfigure api for decoding multiple files)" << std::endl
        << "-maxwidth W          (Max width of all files in list.txt if using reconfigure)" << std::endl
        << "-maxheight H         (Max Height of all files in list.txt if using reconfigure)" << std::endl
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

void ParseCommandLine(std::deque<FILEINFO> *multiFileData, int &maxWidth, int &maxHeight, int &iGpu, bool &useReconfigure, int argc, char *argv[])
{
    FILEINFO fileData;
    char filelistPath[256];

    for (int i = 1; i < argc; i++) {
        if (!_stricmp(argv[i], "-h") || !_stricmp(argv[i], "-help")) {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-filelist")) {
            if (++i == argc) {
                ShowHelpAndExit("-filelist");
            }
            sprintf(filelistPath, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-usereconfigure")) {
            if (++i == argc) {
                ShowHelpAndExit("-usereconfigure");
            }
            useReconfigure = atoi(argv[i]) ? true : false;
            continue;
        }
        if (!_stricmp(argv[i], "-maxwidth")) {
            if (++i == argc) {
                ShowHelpAndExit("-maxwidth");
            }
            maxWidth = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-maxheight")) {
            if (++i == argc) {
                ShowHelpAndExit("-maxheight");
            }
            maxHeight = atoi(argv[i]);
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }

    // Parse the input filelist
    std::ifstream filestream(filelistPath);
    std::string line;
    char* str;
    char param[256];
    char value[256];
    int fileIdx = 0;
    while (std::getline(filestream, line))
    {
        str = (char *)line.c_str();
        sscanf(str,"%s %s", param, value);
        if (0 == _stricmp(param, "infile"))
        {
            if (fileIdx > 0)
            {
                multiFileData->push_back(fileData);
            }
            sprintf(fileData.inFile, "%s", value);
            fileIdx++;
            fileData.resizeDim = { 0, 0 };
            fileData.cropRect = { 0, 0, 0, 0 };
            fileData.outplanar = 0;
        }
        else if (0 == _stricmp(param, "outfile"))
        {
            sprintf(fileData.outFile, "%s", value);
        }
        else if (0 == _stricmp(param, "outplanar"))
        {
            fileData.outplanar = atoi(value) ? true : false;
        }
        else if (0 == _stricmp(param, "resize"))
        {
            sscanf(value, "%dx%d", &fileData.resizeDim.w, &fileData.resizeDim.h);
            if (fileData.resizeDim.w % 2 == 1 || fileData.resizeDim.h % 2 == 1) {
                std::cout << "Resizing rect must have width and height of even numbers" << std::endl;
                exit(1);
            }
        }
        else if (0 == _stricmp(param, "crop"))
        {
            sscanf(value, "%d,%d,%d,%d", &fileData.cropRect.l, &fileData.cropRect.t, &fileData.cropRect.r, &fileData.cropRect.b);
            if ((fileData.cropRect.r - fileData.cropRect.l) % 2 == 1 || (fileData.cropRect.b - fileData.cropRect.t) % 2 == 1) {
                std::cout << "Cropping rect must have width and height of even numbers" << std::endl;
                exit(1);
            }
        }
    }
    if (fileIdx > 0)
    {
        multiFileData->push_back(fileData);
    }
}

/**
*  This sample application illustrates the decoding of multiple files with/without using the decoder Reconfigure API 
*  and display the time taken for decoder creation and destruction. The application supports both planar (YUV420P and YUV420P16)
*  and non-planar (NV12 and P016) output formats.
*/

int main(int argc, char **argv) 
{
    std::deque<FILEINFO> multiFileData;

    int iGpu = 0;
    int maxWidth = 0, maxHeight = 0;
    bool useReconfigure = true;
    FILEINFO fileData;
    try
    {
        ParseCommandLine(&multiFileData, maxWidth, maxHeight, iGpu, useReconfigure, argc, argv);

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
        NvDecoder* dec = NULL;

        while (!multiFileData.empty())
        {
            fileData = multiFileData.front();
            multiFileData.pop_front();

            CheckInputFile(fileData.inFile);

            DecodeMediaFile(cuContext, &dec, fileData, useReconfigure, maxWidth, maxHeight);

        }

        if (dec != NULL)
        {
            delete dec;
            dec = NULL;
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
   
    return 0;
}
