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
#include <exception>
#include <stdexcept>
#include <memory>
#include <functional>
#include <stdint.h>
#include "NvDecoder/NvDecoder.h"
#include "NvEncoder/NvEncoderCuda.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegStreamer.h"
#include "../Utils/FFmpegDemuxer.h"

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
        << "-if          Input format: iyuv nv12 p010 bgra bgra64" << std::endl
        << "-of          Output format: native(nv12/p010) bgra bgra64" << std::endl
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

enum OutputFormat
{
    native = 0, bgra, bgra64
};

std::vector<std::string> vstrOutputFormatName = 
{
    "native", "bgra", "bgra64"
};

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, int &nWidth, int &nHeight,
    NV_ENC_BUFFER_FORMAT &eInputFormat, OutputFormat &eOutputFormat, char *szOutputFileName,
    NvEncoderInitParam &initParam, int &iGpu)
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
        if (!_stricmp(argv[i], "-s")) {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &nWidth, &nHeight)) {
                ShowHelpAndExit("-s");
            }
            continue;
        }
        std::vector<std::string> vszFileFormatName = {
            "iyuv", "nv12", "p010", "bgra", "bgra64"
        };
        NV_ENC_BUFFER_FORMAT aFormat[] = {
            NV_ENC_BUFFER_FORMAT_IYUV,
            NV_ENC_BUFFER_FORMAT_NV12,
            NV_ENC_BUFFER_FORMAT_YUV420_10BIT,
            NV_ENC_BUFFER_FORMAT_ARGB,
            NV_ENC_BUFFER_FORMAT_UNDEFINED,
        };
        if (!_stricmp(argv[i], "-if")) {
            if (++i == argc) {
                ShowHelpAndExit("-if");
            }
            auto it = std::find(vszFileFormatName.begin(), vszFileFormatName.end(), argv[i]);
            if (it == vszFileFormatName.end()) {
                ShowHelpAndExit("-if");
            }
            eInputFormat = aFormat[it - vszFileFormatName.begin()];
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
        // Regard as encoder parameter
        if (argv[i][0] != '-') {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-') {
            oss << argv[++i] << " ";
        }
    }
    // Set VUI parameters for HDR
    std::function<void(NV_ENC_INITIALIZE_PARAMS *pParams)> funcInit = [](NV_ENC_INITIALIZE_PARAMS *pParam)
    {
        if (pParam->encodeGUID == NV_ENC_CODEC_HEVC_GUID)
        {
            NV_ENC_CONFIG_HEVC_VUI_PARAMETERS &hevcVUIParameters = pParam->encodeConfig->encodeCodecConfig.hevcConfig.hevcVUIParameters;
            hevcVUIParameters.videoSignalTypePresentFlag = 1;
            hevcVUIParameters.colourDescriptionPresentFlag = 1;
            hevcVUIParameters.colourMatrix = 4;
        }
        else
        {
            NV_ENC_CONFIG_H264_VUI_PARAMETERS &h264VUIParameters = pParam->encodeConfig->encodeCodecConfig.h264Config.h264VUIParameters;
            h264VUIParameters.videoSignalTypePresentFlag = 1;
            h264VUIParameters.colourDescriptionPresentFlag = 1;
            h264VUIParameters.colourMatrix = 4;
        }
    };
    initParam = NvEncoderInitParam(oss.str().c_str(), (eInputFormat == NV_ENC_BUFFER_FORMAT_UNDEFINED) ? &funcInit : NULL);
}

void EncodeProc(CUdevice cuDevice, int nWidth, int nHeight, NV_ENC_BUFFER_FORMAT eFormat, NvEncoderInitParam *pEncodeCLIOptions,
    bool bBgra64, const char *szInFilePath, const char *szMediaPath, std::exception_ptr &encExceptionPtr) 
{
    CUdeviceptr dpFrame = 0, dpBgraFrame = 0;
    CUcontext cuContext = NULL;

    try
    {
        ck(cuCtxCreate(&cuContext, 0, cuDevice));
        NvEncoderCuda enc(cuContext, nWidth, nHeight, eFormat);
        NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        enc.CreateDefaultEncoderParams(&initializeParams, pEncodeCLIOptions->GetEncodeGUID(), pEncodeCLIOptions->GetPresetGUID());

        pEncodeCLIOptions->SetInitParams(&initializeParams, eFormat);

        enc.CreateEncoder(&initializeParams);

        std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
        if (!fpIn)
        {
            std::cout << "Unable to open input file: " << szInFilePath << std::endl;
            return;
        }

        int nHostFrameSize = bBgra64 ? nWidth * nHeight * 8 : enc.GetFrameSize();
        std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nHostFrameSize]);
        CUdeviceptr dpBgraFrame = 0;
        ck(cuMemAlloc(&dpBgraFrame, nWidth * nHeight * 8));
        int nFrame = 0;
        std::streamsize nRead = 0;
        FFmpegStreamer streamer(pEncodeCLIOptions->IsCodecH264() ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC, nWidth, nHeight, 25, szMediaPath);
        do {
            std::vector<std::vector<uint8_t>> vPacket;
            nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nHostFrameSize).gcount();
            if (nRead == nHostFrameSize)
            {
                const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();

                if (bBgra64)
                {
                    // Color space conversion
                    ck(cuMemcpyHtoD(dpBgraFrame, pHostFrame.get(), nHostFrameSize));
                    Bgra64ToP016((uint8_t *)dpBgraFrame, nWidth * 8, (uint8_t *)encoderInputFrame->inputPtr, encoderInputFrame->pitch, nWidth, nHeight);
                }
                else
                {
                    NvEncoderCuda::CopyToDeviceFrame(cuContext, pHostFrame.get(), 0, (CUdeviceptr)encoderInputFrame->inputPtr,
                        (int)encoderInputFrame->pitch,
                        enc.GetEncodeWidth(),
                        enc.GetEncodeHeight(),
                        CU_MEMORYTYPE_HOST,
                        encoderInputFrame->bufferFormat,
                        encoderInputFrame->chromaOffsets,
                        encoderInputFrame->numChromaPlanes);
                }
                enc.EncodeFrame(vPacket);
            }
            else
            {
                enc.EndEncode(vPacket);
            }
            for (std::vector<uint8_t> &packet : vPacket) {
                streamer.Stream(packet.data(), (int)packet.size(), nFrame++);
            }
        } while (nRead == nHostFrameSize);
        ck(cuMemFree(dpBgraFrame));
        dpBgraFrame = 0;

        enc.DestroyEncoder();
        fpIn.close();

        std::cout << std::flush << "Total frames encoded: " << nFrame << std::endl << std::flush;
    }
    catch (const std::exception& )
    {
        encExceptionPtr = std::current_exception();
        ck(cuMemFree(dpBgraFrame));
        dpBgraFrame = 0;
        ck(cuMemFree(dpFrame));
        dpFrame = 0;
    }
}

void DecodeProc(CUdevice cuDevice, const char *szMediaUri, OutputFormat eOutputFormat, const char *szOutFilePath, std::exception_ptr &decExceptionPtr)
{
    CUdeviceptr dpRgbFrame = 0;
    try
    {
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        FFmpegDemuxer demuxer(szMediaUri);
        // Output host frame for native format; otherwise output device frame for CUDA processing
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), eOutputFormat != native, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), NULL, true);

        uint8_t *pVideo = NULL;
        int nVideoBytes = 0;
        int nFrame = 0;
        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::ostringstream err;
            err << "Unable to open output file: " << szOutFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }

        const char *szTail = "\xe0\x00\x00\x00\x01\xce\x8c\x4d\x9d\x10\x8e\x25\xe9\xfe";
        int nWidth = demuxer.GetWidth(), nHeight = demuxer.GetHeight();
        std::unique_ptr<uint8_t[]> pRgbFrame;
        int nRgbFramePitch = 0, nRgbFrameSize = 0;
        if (eOutputFormat != native) {
            nRgbFramePitch = nWidth * (eOutputFormat == bgra ? 4 : 8);
            nRgbFrameSize = nRgbFramePitch * nHeight;
            pRgbFrame.reset(new uint8_t[nRgbFrameSize]);
            ck(cuMemAlloc(&dpRgbFrame, nRgbFrameSize));
        }
        do {
            demuxer.Demux(&pVideo, &nVideoBytes);
            uint8_t **ppFrame;
            int nFrameReturned = 0;
            dec.Decode(nVideoBytes > 0 ? pVideo + 6 : NULL,
                // Cut head and tail generated by FFmpegDemuxer
                nVideoBytes - (nVideoBytes > 20 && !memcmp(pVideo + nVideoBytes - 14, szTail, 14) ? 20 : 6),
                &ppFrame, &nFrameReturned, CUVID_PKT_ENDOFPICTURE);
            int iMatrix = dec.GetVideoFormatInfo().video_signal_description.matrix_coefficients;
            if (!nFrame) {
                LOG(INFO) << "Color matrix coefficient: " << iMatrix;
            }
            for (int i = 0; i < nFrameReturned; i++) {
                if (eOutputFormat == native) {
                    fpOut.write(reinterpret_cast<char*>(ppFrame[i]), dec.GetFrameSize());
                }
                else {
                    // Color space conversion
                    if (dec.GetBitDepth() == 8) {
                        if (eOutputFormat == bgra) {
                            Nv12ToBgra32(ppFrame[i], nWidth, (uint8_t *)dpRgbFrame, nRgbFramePitch, nWidth, nHeight, iMatrix);
                        }
                        else {
                            Nv12ToBgra64(ppFrame[i], nWidth, (uint8_t *)dpRgbFrame, nRgbFramePitch, nWidth, nHeight, iMatrix);
                        }
                    }
                    else {
                        if (eOutputFormat == bgra) {
                            P016ToBgra32(ppFrame[i], nWidth * 2, (uint8_t *)dpRgbFrame, nRgbFramePitch, nWidth, nHeight, iMatrix);
                        }
                        else {
                            P016ToBgra64(ppFrame[i], nWidth * 2, (uint8_t *)dpRgbFrame, nRgbFramePitch, nWidth, nHeight, iMatrix);
                        }
                    }
                    ck(cuMemcpyDtoH(pRgbFrame.get(), dpRgbFrame, nRgbFrameSize));
                    fpOut.write(reinterpret_cast<char*>(pRgbFrame.get()), nRgbFrameSize);
                }
                nFrame++;
            }
        } while (nVideoBytes);
        if (eOutputFormat != native) 
        {
            ck(cuMemFree(dpRgbFrame));
            dpRgbFrame = 0;
            pRgbFrame.reset(nullptr);
        }
        fpOut.close();

        std::cout << "Total frame decoded: " << nFrame << std::endl
            << "Saved in file " << szOutFilePath << " in "
            << (eOutputFormat == native ? (dec.GetBitDepth() == 8 ? "nv12" : "p010") : (eOutputFormat == bgra ? "bgra" : "bgra64"))
            << " format" << std::endl;
    }
    catch (const std::exception &)
    {
        decExceptionPtr = std::current_exception();
        cuMemFree(dpRgbFrame);
    }
}


/**
*  This sample application illustrates the encoding and streaming of a video
*  with one thread while another thread receives and decodes the video.
*  HDR video streaming is also demonstrated in this application.
*/
int main(int argc, char **argv)
{
    char szInFilePath[256] = "",
        szOutFilePath[256] = "";
    int nWidth = 1920, nHeight = 1080;
    NV_ENC_BUFFER_FORMAT eInputFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    OutputFormat eOutputFormat = native;
    int iGpu = 0;
    bool bBgra64 = false;
    std::exception_ptr encExceptionPtr;
    std::exception_ptr decExceptionPtr;
    try
    {
        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eInputFormat, eOutputFormat, szOutFilePath, encodeCLIOptions, iGpu);

        CheckInputFile(szInFilePath);

        if (eInputFormat == NV_ENC_BUFFER_FORMAT_UNDEFINED) {
            bBgra64 = true;
            eInputFormat = NV_ENC_BUFFER_FORMAT_YUV420_10BIT;
        }
        if (!*szOutFilePath) {
            sprintf(szOutFilePath, "out.%s", eOutputFormat != native ?
                vstrOutputFormatName[eOutputFormat].c_str() :
                (eInputFormat != NV_ENC_BUFFER_FORMAT_YUV420_10BIT ? "nv12" : "p010"));
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

        const char *szMediaUri = "tcp://127.0.0.1:8899";
        char szMediaUriDecode[1024];
        sprintf(szMediaUriDecode, "%s?listen", szMediaUri);
        NvThread thDecode(std::thread(DecodeProc, cuDevice, szMediaUriDecode, eOutputFormat, szOutFilePath, std::ref(decExceptionPtr)));
        NvThread thEncode(std::thread(EncodeProc, cuDevice, nWidth, nHeight, eInputFormat, &encodeCLIOptions, bBgra64, szInFilePath, szMediaUri, std::ref(encExceptionPtr)));
        thEncode.join();
        thDecode.join();
        if (encExceptionPtr)
        {
            std::rethrow_exception(encExceptionPtr);
        }
        if (decExceptionPtr)
        {
            std::rethrow_exception(decExceptionPtr);
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
