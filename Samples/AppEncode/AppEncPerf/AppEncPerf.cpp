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

#include <stdio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cuda.h>
#include <memory>
#include "NvEncoder/NvEncoderCuda.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void EncProc(NvEncoder *pEnc, uint8_t *pBuf, uint32_t nBufSize, uint32_t nFrameTotal,
    std::exception_ptr &encException)
{
    try
    {
        std::vector<std::vector<uint8_t>> vPacket;
        uint64_t nFrameSize = pEnc->GetFrameSize();
        uint32_t n = static_cast<uint32_t>(nBufSize / nFrameSize);
        ck(cuCtxSetCurrent((CUcontext)pEnc->GetDevice()));
        for (uint32_t i = 0; i < nFrameTotal; i++)
        {
            uint32_t iFrame = i / n % 2 ? (n - i % n - 1) : i % n;
            const NvEncInputFrame* encoderInputFrame = pEnc->GetNextInputFrame();
            NvEncoderCuda::CopyToDeviceFrame((CUcontext)pEnc->GetDevice(),
                pBuf + iFrame * nFrameSize,
                0,
                (CUdeviceptr)encoderInputFrame->inputPtr,
                encoderInputFrame->pitch,
                pEnc->GetEncodeWidth(),
                pEnc->GetEncodeHeight(),
                CU_MEMORYTYPE_DEVICE,
                encoderInputFrame->bufferFormat,
                encoderInputFrame->chromaOffsets,
                encoderInputFrame->numChromaPlanes, true);

            pEnc->EncodeFrame(vPacket);
        }
        pEnc->EndEncode(vPacket);
    }
    catch (const std::exception&)
    {
        encException = std::current_exception();
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
        << "-s           Input resolution in this form: WxH" << std::endl
        << "-if          Input format: iyuv nv12 yuv444 p010 yuv444p16 bgra" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        << "-frame       Number of frames to encode per thread (default is 1000)" << std::endl
        << "-thread      Number of encoding thread (default is 2)" << std::endl
        << "-single      (No value) Use single context (this may result in suboptimal performance; default is multiple contexts)" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage();
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
    NV_ENC_BUFFER_FORMAT &eFormat, int &iGpu, uint32_t &nFrame, int &nThread, 
    bool &bSingle, NvEncoderInitParam &initParam) 
{
    std::ostringstream oss;
    for (int i = 1; i < argc; i++)
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
            "iyuv", "nv12", "yv12", "yuv444", "p010", "yuv444p16", "bgra", "argb10", "ayuv", "abgr", "abgr10"
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
            if (++i == argc)
            {
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
        if (!_stricmp(argv[i], "-frame"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-frame");
            }
            nFrame = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-thread"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-thread");
            }
            nThread = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-single"))
        {
            bSingle = true;
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
*  This sample application measures encoding performance in FPS.
*  The application creates multiple host threads and runs a different encoding session
*  on each thread. The number of threads can be controlled by the CLI option "-thread".
*  The application creates 2 host threads, each with a separate encode session, by
*  default. Note that on systems with GeForce GPUs, the number of simultaneous encode
*  sessions allowed on the system is restricted to 2 sessions.
*/

int main(int argc, char **argv)
{
    char szInFilePath[256] = "";
    int nWidth = 1920, nHeight = 1080;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    int iGpu = 0;
    uint32_t nFrame = 1000;
    int nThread = 2;
    bool bSingle = false;
    std::vector<std::exception_ptr> vExceptionPtrs;
    std::vector<CUdeviceptr> vdpBuf;
    using NvEncPtr = std::unique_ptr<NvEncoder, std::function<void(NvEncoder*)>>;
    auto EncodeDeleteFunc = [](NvEncoder *pEnc)
    {
        if (pEnc)
        {
            pEnc->DestroyEncoder();
            delete pEnc;
        }
    };
    try
    {
        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eFormat,
            iGpu, nFrame, nThread, bSingle, encodeCLIOptions);

        CheckInputFile(szInFilePath);

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

        uint8_t *pBuf = NULL;
        uint32_t nBufSize = 0;
        BufferedFileReader bufferedFileReader(szInFilePath, true);
        if (!bufferedFileReader.GetBuffer(&pBuf, &nBufSize)) {
            std::cout << "Failed to read file " << szInFilePath << std::endl;
            return 1;
        }

        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, CU_CTX_SCHED_BLOCKING_SYNC, cuDevice));

        std::vector<CUdeviceptr> vdpBuf;


        std::vector<NvEncPtr> vEnc;
        CUdeviceptr dpBuf;
        ck(cuMemAlloc(&dpBuf, nBufSize));
        vdpBuf.push_back(dpBuf);
        ck(cuMemcpyHtoD(dpBuf, pBuf, nBufSize));
        NvEncPtr pEnc(new NvEncoderCuda(cuContext, nWidth, nHeight, eFormat), EncodeDeleteFunc);

        NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
        initializeParams.encodeConfig = &encodeConfig;
        pEnc->CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions.GetEncodeGUID(), encodeCLIOptions.GetPresetGUID());

        encodeCLIOptions.SetInitParams(&initializeParams, eFormat);

        pEnc->CreateEncoder(&initializeParams);
        vEnc.push_back(std::move(pEnc));


        for (int i = 1; i < nThread; i++)
        {
            if (!bSingle) {
                ck(cuCtxCreate(&cuContext, CU_CTX_SCHED_BLOCKING_SYNC, cuDevice));
                CUdeviceptr dpBuf;
                ck(cuMemAlloc(&dpBuf, nBufSize));
                vdpBuf.push_back(dpBuf);
                ck(cuMemcpyHtoD(vdpBuf[i], pBuf, nBufSize));
            }
            NvEncPtr pEncoder(new NvEncoderCuda(cuContext, nWidth, nHeight, eFormat), EncodeDeleteFunc);
            // all the encoder instances share the same config params , so just use the parameters from first encoder instance
            pEncoder->CreateEncoder(&initializeParams);

            vEnc.push_back(std::move(pEncoder));
        }

        std::vector<NvThread> vThread;
        vExceptionPtrs.resize(nThread);
        StopWatch w;
        w.Start();
        for (int i = 0; i < nThread; i++) 
        {
            vThread.push_back(NvThread(std::thread(EncProc,
                vEnc[i].get(), 
                (uint8_t *)(bSingle ? dpBuf : vdpBuf[i]),
                nBufSize, nFrame, 
                std::ref(vExceptionPtrs[i]))));
        }

        for (auto& t : vThread)
            t.join();

        double t = w.Stop();

        for (int i = 0; i < nThread; i++)
        {
            ck(cuCtxSetCurrent((CUcontext)vEnc[i]->GetDevice()));
            if (!bSingle && i > 0)
            {
                ck(cuMemFree(vdpBuf[i]));
                vdpBuf[i] = 0;
            }
            if (vEnc[i])
            {
                vEnc[i]->DestroyEncoder();
            }
        }
        vdpBuf.clear();

        for (int i = 0; i < nThread; i++)
        {
            if (vExceptionPtrs[i])
                std::rethrow_exception(vExceptionPtrs[i]);
        }

        if (t)
        {
            int nTotal = nFrame * nThread;
            std::cout << "nTotal=" << nTotal << ", time=" << t << " seconds, FPS=" << nTotal / t << std::endl;
        }
    }
    catch (const std::exception &ex)
    {
        for (CUdeviceptr dpBuf : vdpBuf)
            cuMemFree(dpBuf);

        std::cout << ex.what();
        exit(1);
    }

    return 0;
}
