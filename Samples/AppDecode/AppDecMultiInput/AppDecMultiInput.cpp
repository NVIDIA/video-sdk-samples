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
#include <memory>
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_profiler_api.h>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../Common/AppDecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void LaunchRipple(cudaStream_t stream, uint8_t *dpImage, int nWidth, int nHeight, int xCenter, int yCenter, int iTime);
void LaunchOverlayRipple(cudaStream_t stream, uint8_t *dpNv12, uint8_t *dpRipple, int nWidth, int nHeight);
void LaunchMerge(cudaStream_t stream, uint8_t *dpNv12Merged, uint8_t **pdpNv12, int nImage, int nWidth, int nHeight);

void DecProc(NvDecoder *pDec, const char *szInFilePath, int nWidth, int nHeight, uint8_t **apFrameBuffer,
    int nFrameBuffer, int *piEnd, int *piHead, bool *pbStop, cudaStream_t stream, 
    int xCenter, int yCenter, std::exception_ptr &ex) 
{
    try
    {
        FFmpegDemuxer demuxer(szInFilePath);
        ck(cuCtxSetCurrent(pDec->GetContext()));
        uint8_t *dpRippleImage;
        ck(cudaMalloc(&dpRippleImage, nWidth * nHeight));
        int iTime = 0;
        // Render a ripple image on dpRippleImage
        LaunchRipple(stream, dpRippleImage, nWidth, nHeight, xCenter, yCenter, iTime++);
        int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
        uint8_t *pVideo = NULL, **ppFrame;

        do
        {
            demuxer.Demux(&pVideo, &nVideoBytes);
            pDec->DecodeLockFrame(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);

            for (int i = 0; i < nFrameReturned; i++) {
                // For each decoded frame
                while (*piHead == *piEnd) {
                    // Queue is full
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                // Frame buffer is locked, so no data copy is needed here
                apFrameBuffer[*piHead % nFrameBuffer] = ppFrame[i];
                // Overlay dpRippleImage onto the frame buffer
                LaunchOverlayRipple(stream, apFrameBuffer[*piHead % nFrameBuffer], dpRippleImage, nWidth, nHeight);
                // Make sure CUDA kernel is finished before marking the current position as ready
                ck(cudaStreamSynchronize(stream));
                // Mark as ready
                ++*piHead;
                LaunchRipple(stream, dpRippleImage, nWidth, nHeight, xCenter, yCenter, iTime++);
            }
        } while (nVideoBytes);

        ck(cudaFree(dpRippleImage));
        *pbStop = true;
    }
    catch (std::exception&)
    {
        ex = std::current_exception();
    }
}

/**
*  This sample application demonstrates shows how to decode multiple raw video files and
*  post-process them with CUDA kernels on different CUDA streams.
*/

int main(int argc, char *argv[])
{
    char szInFilePath[256] = "", szOutFilePath[256] = "out.nv12";
    int iGpu = 0;
    std::vector<std::exception_ptr> vExceptionPtrs;
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

        FFmpegDemuxer demuxer(szInFilePath);
        int nWidth = demuxer.GetWidth(), nHeight = demuxer.GetHeight(), nByte = nWidth * nHeight * 3 / 2;

        // Number of decoders
        const int n = 4;
        // Every decoder has its own round queue
        uint8_t *aapFrameBuffer[n][8];
        // Queue capacity
        const int nFrameBuffer = sizeof(aapFrameBuffer[0]) / sizeof(aapFrameBuffer[0][0]);
        int iEnd = nFrameBuffer;
        bool abStop[n] = {};
        int aiHead[n] = {};
        std::vector <NvThread> vThreads;
        std::vector <std::unique_ptr<NvDecoder>> vDecoders;
        // Coordinate of the ripple center for each decoder
        int axCenter[] = { nWidth / 4, nWidth / 4 * 3, nWidth / 4, nWidth / 4 * 3 };
        int ayCenter[] = { nHeight / 4, nHeight / 4, nHeight / 4 * 3, nHeight / 4 * 3 };
        cudaStream_t aStream[n];
        vExceptionPtrs.resize(n);
        for (int i = 0; i < n; i++)
        {
            ck(cudaStreamCreate(&aStream[i]));
            std::unique_ptr<NvDecoder> dec(new NvDecoder(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec())));
            vDecoders.push_back(std::move(dec));
            vThreads.push_back(NvThread(std::thread(DecProc, vDecoders[i].get(), szInFilePath, nWidth, nHeight, aapFrameBuffer[i],
                nFrameBuffer, &iEnd, aiHead + i, abStop + i, aStream[i], axCenter[i], ayCenter[i], std::ref(vExceptionPtrs[i]))));
        }

        std::unique_ptr<uint8_t[]> pImage(new uint8_t[nByte]);
        uint8_t* dpImage = nullptr;
        ck(cudaMalloc(&dpImage, nByte));
        std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
        if (!fpOut)
        {
            std::ostringstream err;
            err << "Unable to open output file: " << szOutFilePath << std::endl;
            throw std::invalid_argument(err.str());
        }

        int nFrame = 0;
        for (int i = 0;; i++)
        {
            // For each decoded frame #i
            // iHead is used for ensuring all decoders have made progress
            int iHead = INT_MAX;
            for (int j = 0; j < n; j++)
            {
                while (!abStop[j] && aiHead[j] <= i)
                {
                    // Decoder #j hasn't decoded frame #i
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                iHead = (std::min)(iHead, aiHead[j]);
            }
            if (iHead <= i)
            {
                // Some decoder stops
                nFrame = i;
                break;
            }

            std::cout << "Merge frames at #" << i << "\r";
            uint8_t *apNv12[] = { aapFrameBuffer[0][i % nFrameBuffer], aapFrameBuffer[1][i % nFrameBuffer], aapFrameBuffer[2][i % nFrameBuffer], aapFrameBuffer[3][i % nFrameBuffer] };
            // Merge all frames into dpImage
            LaunchMerge(0, dpImage, apNv12, n, nWidth, nHeight);
            ck(cudaMemcpy(pImage.get(), dpImage, nByte, cudaMemcpyDeviceToHost));
            fpOut.write(reinterpret_cast<char*>(pImage.get()), nByte);

            for (int j = 0; j < n; j++)
            {
                vDecoders[j]->UnlockFrame(&aapFrameBuffer[j][i % nFrameBuffer], 1);
            }
            iEnd++;
        }
        fpOut.close();
        ck(cudaFree(dpImage));

        for (int i = 0; i < n; i++)
        {
            if (vExceptionPtrs[i])
            {
                std::rethrow_exception(vExceptionPtrs[i]);
            }
        }

        ck(cudaProfilerStop());
        if (nFrame)
        {
            std::cout << "Merged video saved in " << szOutFilePath << ". A total of " << nFrame << " frames were decoded." << std::endl;
            return 0;
        }
        else
        {
            std::cout << "Warning: no video frame decoded. Please don't use container formats (such as mp4/avi/webm) as the input, but use raw elementary stream file instead." << std::endl;
            return 1;
        }
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
