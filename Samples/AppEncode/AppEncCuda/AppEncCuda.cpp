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

#include <fstream>
#include <iostream>
#include <memory>
#include <cuda.h>
#include "NvEncoder/NvEncoderCuda.h"
#include "../Utils/Logger.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void EncodeCuda(CUcontext cuContext, char *szInFilePath, int nWidth, int nHeight, NV_ENC_BUFFER_FORMAT eFormat,
    char *szOutFilePath, NvEncoderInitParam *pEncodeCLIOptions)
{
    std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpIn)
    {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    NvEncoderCuda enc(cuContext, nWidth, nHeight, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, pEncodeCLIOptions->GetEncodeGUID(), pEncodeCLIOptions->GetPresetGUID());

    pEncodeCLIOptions->SetInitParams(&initializeParams, eFormat);

    enc.CreateEncoder(&initializeParams);

    int nFrameSize = enc.GetFrameSize();

    std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nFrameSize]);
    int nFrame = 0;
    while (true)
    {
        // Load the next frame from disk
        std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();
        // For receiving encoded packets
        std::vector<std::vector<uint8_t>> vPacket;
        if (nRead == nFrameSize)
        {
            const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();
            NvEncoderCuda::CopyToDeviceFrame(cuContext, pHostFrame.get(), 0, (CUdeviceptr)encoderInputFrame->inputPtr,
                (int)encoderInputFrame->pitch,
                enc.GetEncodeWidth(),
                enc.GetEncodeHeight(), 
                CU_MEMORYTYPE_HOST, 
                encoderInputFrame->bufferFormat,
                encoderInputFrame->chromaOffsets,
                encoderInputFrame->numChromaPlanes);

            enc.EncodeFrame(vPacket);
        }
        else
        {
            enc.EndEncode(vPacket);
        }
        nFrame += (int)vPacket.size();
        for (std::vector<uint8_t> &packet : vPacket)
        {
            // For each encoded packet
            fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        }

        if (nRead != nFrameSize) break;
    }

    enc.DestroyEncoder();
    fpOut.close();
    fpIn.close();

    std::cout << "Total frames encoded: " << nFrame << std::endl << "Saved in file " << szOutFilePath << std::endl;
}

void ShowEncoderCapability()
{
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    printf("Encoder Capability\n");
    printf("#  %-20.20s H264 H264_444 H264_ME H264_WxH  HEVC HEVC_Main10 HEVC_Lossless HEVC_SAO HEVC_444 HEVC_ME HEVC_WxH\n", "GPU");
    for (int iGpu = 0; iGpu < nGpu; iGpu++) {
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));
        NvEncoderCuda enc(cuContext, 1280, 720, NV_ENC_BUFFER_FORMAT_NV12);

        //Adjusted # %-20.20s H264  H264_444  H264_ME  H264_WxH HEVC  HEVC_Main10  HEVC_Lossless  HEVC_SAO  HEVC_444  HEVC_ME  HEVC_WxH
        printf("%-2d %-20.20s   %s      %s       %s    %4dx%-4d   %s       %s            %s           %s        %s       %s    %4dx%-4d\n", 
            iGpu, szDeviceName,
            enc.GetCapabilityValue(NV_ENC_CODEC_H264_GUID, NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_H264_GUID, NV_ENC_CAPS_SUPPORT_YUV444_ENCODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_H264_GUID, NV_ENC_CAPS_SUPPORT_MEONLY_MODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_H264_GUID, NV_ENC_CAPS_WIDTH_MAX),
            enc.GetCapabilityValue(NV_ENC_CODEC_H264_GUID, NV_ENC_CAPS_HEIGHT_MAX),
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORT_10BIT_ENCODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORT_SAO) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORT_YUV444_ENCODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_SUPPORT_MEONLY_MODE) ? "+" : "-",
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_WIDTH_MAX),
            enc.GetCapabilityValue(NV_ENC_CODEC_HEVC_GUID, NV_ENC_CAPS_HEIGHT_MAX)
        );

        enc.DestroyEncoder();
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
        << "-i           Input file path" << std::endl
        << "-o           Output file path" << std::endl
        << "-s           Input resolution in this form: WxH" << std::endl
        << "-if          Input format: iyuv nv12 yuv444 p010 yuv444p16 bgra bgra10 ayuv abgr abgr10" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage() << std::endl;
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        ShowEncoderCapability();
        exit(0);
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, int &nWidth, int &nHeight, 
    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam, int &iGpu)
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (!_stricmp(argv[i], "-h"))
        {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-i"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-i");
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-o");
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-s"))
        {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &nWidth, &nHeight))
            {
                ShowHelpAndExit("-s");
            }
            continue;
        }
        std::vector<std::string> vszFileFormatName =
        {
            "iyuv", "nv12", "yv12", "yuv444", "p010", "yuv444p16", "bgra", "bgra10", "ayuv", "abgr", "abgr10"
        };
        NV_ENC_BUFFER_FORMAT aFormat[] = 
        {
            NV_ENC_BUFFER_FORMAT_IYUV,
            NV_ENC_BUFFER_FORMAT_NV12,
            NV_ENC_BUFFER_FORMAT_YV12,
            NV_ENC_BUFFER_FORMAT_YUV444,
            NV_ENC_BUFFER_FORMAT_YUV420_10BIT,
            NV_ENC_BUFFER_FORMAT_YUV444_10BIT,
            NV_ENC_BUFFER_FORMAT_ARGB,
            NV_ENC_BUFFER_FORMAT_ARGB10,
            NV_ENC_BUFFER_FORMAT_AYUV,
            NV_ENC_BUFFER_FORMAT_ABGR,
            NV_ENC_BUFFER_FORMAT_ABGR10,
        };
        if (!_stricmp(argv[i], "-if"))
        {
            if (++i == argc) {
                ShowHelpAndExit("-if");
            }
            auto it = std::find(vszFileFormatName.begin(), vszFileFormatName.end(), argv[i]);
            if (it == vszFileFormatName.end())
            {
                ShowHelpAndExit("-if");
            }
            eFormat = aFormat[it - vszFileFormatName.begin()];
            continue;
        }
        if (!_stricmp(argv[i], "-gpu"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        // Regard as encoder parameter
        if (argv[i][0] != '-')
        {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-')
        {
            oss << argv[++i] << " ";
        }
    }
    initParam = NvEncoderInitParam(oss.str().c_str());
}

/**
*  This sample application illustrates encoding of frames in CUDA device buffers.
*  The application reads the image data from file and loads it to CUDA input
*  buffers obtained from the encoder using NvEncoder::GetNextInputFrame().
*  The encoder subsequently maps the CUDA buffers for encoder using NvEncodeAPI
*  and submits them to NVENC hardware for encoding as part of EncodeFrame() function.
*/

int main(int argc, char **argv)
{
    char szInFilePath[256] = "",
        szOutFilePath[256] = "";
    int nWidth = 1920, nHeight = 1080;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    int iGpu = 0;
    try
    {
        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eFormat, szOutFilePath, encodeCLIOptions, iGpu);

        CheckInputFile(szInFilePath);

        if (!*szOutFilePath)
        {
            sprintf(szOutFilePath, encodeCLIOptions.IsCodecH264() ? "out.h264" : "out.hevc");
        }

        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu)
        {
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

        EncodeCuda(cuContext, szInFilePath, nWidth, nHeight, eFormat, szOutFilePath, &encodeCLIOptions);
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what();
        return 1;
    }
    return 0;
}
