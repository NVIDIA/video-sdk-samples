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
#include <iomanip>
#include <memory>
#include <stdint.h>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"
#include "PSNR.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

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
        << "-if          Input format: iyuv nv12 p010" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage(false, false, true);
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

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, int &nWidth, int &nHeight,
    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam,
    int &iGpu)
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
            "iyuv", "nv12", "p010"
        };
        NV_ENC_BUFFER_FORMAT aFormat[] = 
        {
            NV_ENC_BUFFER_FORMAT_IYUV,
            NV_ENC_BUFFER_FORMAT_NV12,
            NV_ENC_BUFFER_FORMAT_YUV420_10BIT,
        };
        if (!_stricmp(argv[i], "-if"))
        {
            if (++i == argc) 
            {
                ShowHelpAndExit("-if");
            }
            auto it = find(vszFileFormatName.begin(), vszFileFormatName.end(), argv[i]);
            if (it == vszFileFormatName.end())
            {
                ShowHelpAndExit("-if");
            }
            eFormat = aFormat[it - vszFileFormatName.begin()];
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        // Regard as encoder parameter
        if (argv[i][0] != '-') {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-') {
            oss << argv[++i] << " ";
        }
    }
    initParam = NvEncoderInitParam(oss.str().c_str());
}

template <typename YuvUnit>
void EncQual(char *szInFilePath, char *szOutFilePath, int nWidth, int nHeight, NV_ENC_BUFFER_FORMAT eFormat, int iGpu, NvEncoderInitParam &encodeCLIOptions)
{
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    if (iGpu < 0 || iGpu >= nGpu)
    {
        std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
        return;
    }
    CUdevice cuDevice = 0;
    ck(cuDeviceGet(&cuDevice, iGpu));
    char szDeviceName[80];
    ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
    std::cout << "GPU in use: " << szDeviceName << std::endl;
    CUcontext cuContext = NULL;
    ck(cuCtxCreate(&cuContext, 0, cuDevice));

    NvEncoderCuda enc(cuContext, nWidth, nHeight, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions.GetEncodeGUID(), encodeCLIOptions.GetPresetGUID());

    encodeCLIOptions.SetInitParams(&initializeParams, eFormat);

    enc.CreateEncoder(&initializeParams);

    NvDecoder dec(cuContext, nWidth, nHeight, false, encodeCLIOptions.IsCodecH264() ? cudaVideoCodec_H264 : cudaVideoCodec_HEVC);

    int nSize = enc.GetFrameSize();
    std::streamsize nRead = 0;
    const int nFrame = 32;
    std::vector<std::unique_ptr<uint8_t[]>> vEncFrame;
    for (int i = 0; i < nFrame; i++) 
    {
        std::unique_ptr<uint8_t[]> frame(new uint8_t[nSize]);
        vEncFrame.push_back(std::move(frame));
    }

    int iEnc = 0, iDec = 0;
    std::ifstream fpYuv(szInFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpYuv)
    {
        std::cout << "Unable to open input file: " << szInFilePath << std::endl;
        exit(1);
    }

    int64_t eySum = 0, euSum = 0, evSum = 0;
    int64_t eyuvMin = INT64_MAX, eyuvMax = INT64_MIN;
    double sySum = 0, suSum = 0, svSum = 0, syuvSum = 0;
    std::cout << std::setprecision(4) << std::fixed;
    YuvConverter<YuvUnit> converter(nWidth, nHeight);
    std::ofstream fout;
    if (*szOutFilePath)
    {
        fout.open(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fout.is_open())
        {
            std::cout << "Unable to open output file: " << szOutFilePath << std::endl;
            exit(1);
        }
    }
    int MAX, shift;
    if (eFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT)
    {
        MAX = 1023;
        shift = 6;
    }
    else
    {
        MAX = 255;
        shift = 0;
    }

    do 
    {
        nRead = fpYuv.read(reinterpret_cast<char*>(vEncFrame[iEnc % nFrame].get()), nSize).gcount();
        std::vector<std::vector<uint8_t>> vPacket;
        if (nRead == nSize)
        {
            const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();

            NvEncoderCuda::CopyToDeviceFrame(cuContext, vEncFrame[iEnc++ % nFrame].get(), 0, (CUdeviceptr)encoderInputFrame->inputPtr,
                (int)encoderInputFrame->pitch, enc.GetEncodeWidth(), enc.GetEncodeHeight(), CU_MEMORYTYPE_HOST, 
                encoderInputFrame->bufferFormat,
                encoderInputFrame->chromaOffsets,
                encoderInputFrame->numChromaPlanes);

            enc.EncodeFrame(vPacket);
        }
        else
        {
            enc.EndEncode(vPacket);
        }
        std::vector<uint8_t> vTmpPacket;
        for (std::vector<uint8_t> &packet : vPacket)
        {
            vTmpPacket.insert(vTmpPacket.end(), packet.data(), packet.data() + (int)packet.size());
        }

        uint8_t **apDecFrame;
        int nFrameReturned = 0;
        dec.Decode(vTmpPacket.data(), (int)vTmpPacket.size(), &apDecFrame, &nFrameReturned, nRead != nSize);
        for (int i = 0; i < nFrameReturned; i++)
        {
            uint8_t *pEncFrame = vEncFrame[iDec % nFrame].get(), *pDecFrame = apDecFrame[i];
            if (eFormat != NV_ENC_BUFFER_FORMAT_IYUV)
            {
                converter.UVInterleavedToPlanar((YuvUnit *)pEncFrame);
            }
            converter.UVInterleavedToPlanar((YuvUnit *)pDecFrame);
            if (fout.is_open())
            {
                fout.write(reinterpret_cast<char*>(pDecFrame), dec.GetFrameSize());
            }

            int64_t ey, eu, ev;
            SumSquareErrorFor420Planar((YuvUnit *)pEncFrame, (YuvUnit *)pDecFrame, nWidth, nHeight, &ey, &eu, &ev, shift);
            eySum += ey; euSum += eu; evSum += ev;
            int64_t eyuv = ey + eu + ev;
            eyuvMin = (std::min)(eyuvMin, eyuv);
            eyuvMax = (std::max)(eyuvMax, eyuv);
            std::cout << std::setprecision(2);
            std::cout << "n:" << iDec + 1 << " mse_avg:" << 1.0 * eyuv / (nWidth * nHeight * 3 / 2)
                << " mse_y:" << 1.0 * ey / (nWidth * nHeight)
                << " mse_u:" << 1.0 * eu * 4 / (nWidth * nHeight)
                << " mse_v:" << 1.0 * ev * 4 / (nWidth * nHeight)
                << " psnr_avg:" << psnr(eyuv, nWidth * nHeight * 3 / 2, MAX)
                << " psnr_y:" << psnr(ey, nWidth * nHeight, MAX)
                << " psnr_u:" << psnr(eu, nWidth * nHeight / 4, MAX)
                << " psnr_v:" << psnr(ev, nWidth * nHeight / 4, MAX)
                << " " << std::endl;

            iDec++;
        }
    } while (nRead == nSize);
    fout.close();
    fpYuv.close();

    std::cout << std::setprecision(6);
    std::cout << "PSNR y:" << psnr(eySum, (int64_t)nWidth * nHeight * iEnc, MAX)
        << " u:" << psnr(euSum, (int64_t)nWidth * nHeight / 4 * iEnc, MAX)
        << " v:" << psnr(evSum, (int64_t)nWidth * nHeight / 4 * iEnc, MAX)
        << " average:" << psnr(eySum + euSum + evSum, (int64_t)nWidth * nHeight * 3 / 2 * iEnc, MAX)
        << " min:" << psnr(eyuvMax, (int64_t)nWidth * nHeight * 3 / 2, MAX)
        << " max:" << psnr(eyuvMin, (int64_t)nWidth * nHeight * 3 / 2, MAX)
        << std::endl;

    if (*szOutFilePath) {
        std::cout << "Total frame encoded and decoded: " << iDec << std::endl
            << "Saved in file " << szOutFilePath << " in "
            << (dec.GetBitDepth() == 8 ? "iyuv" : "yuv420p16")
            << " format" << std::endl;
    }

    enc.DestroyEncoder();

}

/**
*  This sample application demonstrates measurement of encoding quality, in
*  terms of PSNR. The application encodes frames from the input file and then
*  decodes them, computing PSNR between input and decoded output. The decoded
*  output can be saved to a file by using the "-o" option.
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

        if (eFormat == NV_ENC_BUFFER_FORMAT_YUV420_10BIT)
        {
            EncQual<uint16_t>(szInFilePath, szOutFilePath, nWidth, nHeight, eFormat, iGpu, encodeCLIOptions);
        }
        else
        {
            EncQual<uint8_t>(szInFilePath, szOutFilePath, nWidth, nHeight, eFormat, iGpu, encodeCLIOptions);
        }
    }
    catch (const std::exception &e)
    {
        std::cout << e.what();
    }
    return 0;
}
