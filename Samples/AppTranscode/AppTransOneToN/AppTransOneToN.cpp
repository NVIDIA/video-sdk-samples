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

#include <cuda_runtime.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <string.h>
#include <memory>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"

using NvEncCudaPtr = std::unique_ptr<NvEncoderCuda, std::function<void(NvEncoderCuda*)>>;

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

int FindMin(volatile int *a, int n) 
{
    int r = INT_MAX;
    for (int i = 0; i < n; i++)
    {
        if (a[i] < r)
        {
            r = a[i];
        }
    }
    return r;
}

void EncProc(NvEncoderCuda *pEnc, uint8_t **apSrcFrame, int nSrcFrame, int nSrcFramePitch, int nSrcFrameWidth, int nSrcFrameHeight, bool bOut10,
        volatile int *piEnc, volatile int *piDec, volatile bool *pbEnd, const char *szOutFileNamePrefix, const char *szOutFileNameSuffix, std::exception_ptr & encException, int encoderId)
{
    try
    {
        char szOutFilePath[80];
        sprintf(szOutFilePath, "%s_%dx%d_%d.%s", szOutFileNamePrefix, pEnc->GetEncodeWidth(), pEnc->GetEncodeHeight(), encoderId, szOutFileNameSuffix);
        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::cout << "Unable to open output file: " << szOutFilePath << std::endl;
            return;
        }

        ck(cuCtxSetCurrent((CUcontext)pEnc->GetDevice()));
        CUdeviceptr pFrameResized;
        ck(cuMemAlloc(&pFrameResized, pEnc->GetFrameSize()));

        while (*piEnc != *piDec || !*pbEnd)
        {
            if (*piEnc == *piDec)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            for (; *piEnc < *piDec || *pbEnd; (*piEnc)++)
            {
                std::vector<std::vector<uint8_t>> vPacket;
                if (*piEnc < *piDec)
                {
                    const NvEncInputFrame* encoderInputFrame = pEnc->GetNextInputFrame();
                    if (bOut10)
                    {
                        ResizeP016((unsigned char *)encoderInputFrame->inputPtr, (int)encoderInputFrame->pitch, pEnc->GetEncodeWidth(), pEnc->GetEncodeHeight(),
                            apSrcFrame[*piEnc % nSrcFrame], nSrcFramePitch, nSrcFrameWidth, nSrcFrameHeight);
                    }
                    else
                    {
                        ResizeNv12((unsigned char *)encoderInputFrame->inputPtr, (int)encoderInputFrame->pitch, pEnc->GetEncodeWidth(), pEnc->GetEncodeHeight(),
                            apSrcFrame[*piEnc % nSrcFrame], nSrcFramePitch, nSrcFrameWidth, nSrcFrameHeight);
                    }
                    pEnc->EncodeFrame(vPacket);
                }
                else
                {
                    pEnc->EndEncode(vPacket);
                }
                for (std::vector<uint8_t> packet : vPacket)
                {
                    fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
                }
                if (*piEnc == *piDec && *pbEnd) break;
            }
        }
        ck(cuMemFree(pFrameResized));
        fpOut.close();
    }
    catch (const std::exception&)
    {
        encException = std::current_exception();
    }
}

void TranscodeOneToN(NvDecoder *pDec, FFmpegDemuxer *pDemuxer, std::vector<NvEncCudaPtr>& vEncoders, int nEnc, int *pnFrameTrans,
    const char *szOutFileNamePrefix, const char *szOutFileNameSuffix, std::vector<std::exception_ptr>& vExceptionPtrs)
{
    const int nSrcFrame = 8;

    volatile bool bEnd = false;
    // next frame to be decoded. apFrame[iDec] is unoccupied when iDec - iEnc < nFrame
    volatile int iDec = 0;
    // iEnc=aiEnc[i]: next frame to be encoded. apFrame[iEnc] is eligible for encoding when iEnc < iDec

    uint8_t *apSrcFrame[nSrcFrame] = { 0 };
    std::vector<NvThread> vpth;
    int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
    uint8_t *pVideo = NULL, **ppFrame = NULL;

    std::unique_ptr<int[]> aiEncPtr(new int[nEnc]);
    volatile int *aiEnc = aiEncPtr.get();
    memset((void *)aiEnc, 0, nEnc * sizeof(int));
    do {

        pDemuxer->Demux(&pVideo, &nVideoBytes);
        pDec->DecodeLockFrame(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
        if (nFrameReturned && !apSrcFrame[0])
        {
            for (int i = 0; i < nEnc; i++)
            {
                vpth.push_back(NvThread(std::thread(EncProc, vEncoders[i].get(), apSrcFrame, nSrcFrame,
                    pDec->GetDeviceFramePitch(), pDemuxer->GetWidth(), pDemuxer->GetHeight(), pDemuxer->GetBitDepth() > 8,
                    aiEnc + i, &iDec, &bEnd, szOutFileNamePrefix, szOutFileNameSuffix, std::ref(vExceptionPtrs[i]), i)));
            }
        }
        for (int i = 0; i < nFrameReturned; i++)
        {
            while (iDec - FindMin(aiEnc, nEnc) == nSrcFrame)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (apSrcFrame[iDec % nSrcFrame])
            {
                // Unlock (recycle) the frame buffer before proceeding
                pDec->UnlockFrame(&apSrcFrame[iDec % nSrcFrame], 1);
            }
            // No need for data copy because frame buffer is locked
            apSrcFrame[iDec % nSrcFrame] = ppFrame[i];
            iDec++;
        }
    } while (nVideoBytes);
    bEnd = true;
    for (auto& pth : vpth)
    {
        pth.join();
    }
    for (int i = 0; i < nSrcFrame; i++)
    {
        if (apSrcFrame[i])
        {
            pDec->UnlockFrame(&apSrcFrame[i], 1);
        }
    }

    *pnFrameTrans = iDec;
}

void ShowHelpAndExit(char *szExeName, bool bHelp = false)
{
    std::ostringstream oss;
    oss << "Usage: " << szExeName << std::endl
        << "-i           input_file" << std::endl
        << "-o           output_file" << std::endl 
        << "-r           W1xH1 W2xH2 ..." << std::endl
        << "-gpu         GPU ordinal" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage(false, false, true);
    if (!bHelp)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, char *szOutputFileName, 
    std::vector<int2> &vResolution, int &iGpu, NvEncoderInitParam &initParam) 
{
    std::ostringstream oss;
    for (int i = 1; i < argc; i++) {
        if (!_stricmp(argv[i], "-h")) {
            ShowHelpAndExit(argv[0], true);
        }
        if (!_stricmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit(argv[0]);
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o")) {
            if (++i == argc) {
                ShowHelpAndExit(argv[0]);
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-r")) {
            int w, h;
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &w, &h)) {
                ShowHelpAndExit(argv[0]);
            }
            vResolution.push_back(make_int2(w, h));
            while (++i != argc && 2 == sscanf(argv[i], "%dx%d", &w, &h)) {
                vResolution.push_back(make_int2(w, h));
            }
            if (i != argc) {
                i--;
            }
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit(argv[0]);
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
    // fill default values
    if (vResolution.empty()) {
        vResolution.push_back(make_int2(1280, 720));
        vResolution.push_back(make_int2(800, 480));
    }
}

int main(int argc, char *argv[]) 
{
    int iGpu = 0;
    char szInFilePath[260] = "";
    char szOutFileNamePrefix[260] = "out";
    std::vector<int2> vResolution;
    std::vector<std::exception_ptr> vExceptionPtrs;
    try
    {
        auto EncodeDeleteFunc = [](NvEncoderCuda *pEnc)
        {
            if (pEnc)
            {
                pEnc->DestroyEncoder();
                delete pEnc;
            }
        };
        std::vector<NvEncCudaPtr> vEncoders;

        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, szOutFileNamePrefix, vResolution, iGpu, encodeCLIOptions);

        CheckInputFile(szInFilePath);

        std::cout
            << "Input file             : " << szInFilePath << std::endl
            << "Output file name pefix : " << szOutFileNamePrefix << std::endl
            << "Output resolutions     : ";
        for (int2 xy : vResolution) {
            std::cout << xy.x << "x" << xy.y << " ";
        }
        std::cout << std::endl
            << "GPU ordinal        : " << iGpu << std::endl;

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
        std::cout << "GPU in use         : " << szDeviceName << std::endl;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        FFmpegDemuxer demuxer(szInFilePath);
        int nEnc = (int)vResolution.size();
        for (int i = 0; i < nEnc; i++)
        {
            NvEncCudaPtr encPtr(new NvEncoderCuda(cuContext, vResolution[i].x, vResolution[i].y,
                demuxer.GetBitDepth() == 8 ? NV_ENC_BUFFER_FORMAT_NV12 : NV_ENC_BUFFER_FORMAT_YUV420_10BIT), EncodeDeleteFunc);
            vEncoders.push_back(std::move(encPtr));

            NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
            NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
            initializeParams.encodeConfig = &encodeConfig;
            vEncoders[i]->CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions.GetEncodeGUID(), encodeCLIOptions.GetPresetGUID());
            encodeCLIOptions.SetInitParams(&initializeParams, demuxer.GetBitDepth() == 8 ? NV_ENC_BUFFER_FORMAT_NV12 : NV_ENC_BUFFER_FORMAT_YUV420_10BIT);

            vEncoders[i]->CreateEncoder(&initializeParams);
        }

        int nFrameTrans = 0;
        vExceptionPtrs.resize(nEnc);
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()), NULL, false, true);
        TranscodeOneToN(&dec, &demuxer, vEncoders, nEnc, &nFrameTrans, szOutFileNamePrefix, encodeCLIOptions.IsCodecH264() ? "h264" : "hevc", vExceptionPtrs);

        for (int i = 0; i < nEnc; i++)
        {
            if (vExceptionPtrs[i])
            {
                std::rethrow_exception(vExceptionPtrs[i]);
            }
        }

        std::cout << "Frames transcoded: " << nFrameTrans << " x " << nEnc << std::endl;
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
