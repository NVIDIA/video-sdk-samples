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

#include <stdint.h>
#include <cuda_runtime.h>
#include "../Utils/NvCodecUtils.h"

#define SLEEP_TIME 0

inline __device__ double sleep(int n) {
    double d = 1.0;
    for (int i = 0; i < n; i++) {
        d += sin(d);
    }
    return d;
}

static __global__ void Ripple(uint8_t *pImage, int nWidth, int nHeight, int xCenter, int yCenter, int iTime) {
    int ix = blockIdx.x * blockDim.x + threadIdx.x,
        iy = blockIdx.y * blockDim.y + threadIdx.y;
    if (ix >= nWidth || iy >= nHeight) {
        return;
    }
    float dx = ix - xCenter, dy = iy - yCenter, d = sqrtf(dx * dx + dy * dy), dmax = sqrtf(nWidth * nWidth + nHeight * nHeight) / 2.0f;
    pImage[iy * nWidth + ix] = (uint8_t)(127.0f * (1.0f - d / dmax) * sinf((d - iTime * 10)* 0.1) + 128.0f);
    sleep(SLEEP_TIME);
}

void LaunchRipple(cudaStream_t stream, uint8_t *dpImage, int nWidth, int nHeight, int xCenter, int yCenter, int iTime) {
    Ripple<<<dim3((nWidth + 15) / 16, (nHeight + 15) / 16), dim3(16, 16), 0, stream>>>(dpImage, nWidth, nHeight, xCenter, yCenter, iTime);
}

inline __device__ uint8_t clamp(int i) {
    return (uint8_t)min(max(i, 0), 255);
}

static __global__ void OverlayRipple(uint8_t *pNv12, uint8_t *pRipple, int nWidth, int nHeight) {
    int ix = blockIdx.x * blockDim.x + threadIdx.x,
        iy = blockIdx.y * blockDim.y + threadIdx.y;
    if (ix >= nWidth || iy >= nHeight) {
        return;
    }
    pNv12[iy * nWidth + ix] = clamp(pNv12[iy * nWidth + ix] + (pRipple[iy * nWidth + ix] - 127.0f) * 0.8f);
    sleep(SLEEP_TIME);
}

void LaunchOverlayRipple(cudaStream_t stream, uint8_t *dpNv12, uint8_t *dpRipple, int nWidth, int nHeight) {
    OverlayRipple<<<dim3((nWidth + 15) / 16, (nHeight + 15) / 16), dim3(16, 16), 0, stream>>>(dpNv12, dpRipple, nWidth, nHeight);
}

static __global__ void Merge(uint8_t *pNv12Merged, uint8_t **apNv12, int nImage, int nWidth, int nHeight) {
    int ix = blockIdx.x * blockDim.x + threadIdx.x,
        iy = blockIdx.y * blockDim.y + threadIdx.y;
    if (ix >= nWidth / 2 || iy >= nHeight / 2) {
        return;
    }
    uint2 y01 = {}, y23 = {}, uv = {};
    for (int i = 0; i < nImage; i++) {
        uchar2 c2;
        c2 = *(uchar2 *)(apNv12[i] + nWidth * iy * 2 + ix * 2);
        y01.x += c2.x; y01.y += c2.y;
        c2 = *(uchar2 *)(apNv12[i] + nWidth * (iy * 2 + 1) + ix * 2);
        y23.x += c2.x; y23.y += c2.y;
        c2 = *(uchar2 *)(apNv12[i] + nWidth * (nHeight + iy) + ix * 2);
        uv.x += c2.x; uv.y += c2.y;
    }
    *(uchar2 *)(pNv12Merged + nWidth * iy * 2 + ix * 2) = uchar2 {(uint8_t)(y01.x / nImage), (uint8_t)(y01.y / nImage)};
    *(uchar2 *)(pNv12Merged + nWidth * (iy * 2 + 1) + ix * 2) = uchar2 {(uint8_t)(y23.x / nImage), (uint8_t)(y23.y / nImage)};
    *(uchar2 *)(pNv12Merged + nWidth * (nHeight + iy) + ix * 2) = uchar2 {(uint8_t)(uv.x / nImage), (uint8_t)(uv.y / nImage)};
    sleep(SLEEP_TIME);
}

void LaunchMerge(cudaStream_t stream, uint8_t *dpNv12Merged, uint8_t **pdpNv12, int nImage, int nWidth, int nHeight) {
    uint8_t **dadpNv12;
    ck(cudaMalloc(&dadpNv12, sizeof(uint8_t *) * nImage));
    ck(cudaMemcpy(dadpNv12, pdpNv12, sizeof(uint8_t *) * nImage, cudaMemcpyHostToDevice));
    Merge<<<dim3((nWidth + 15) / 16, (nHeight + 15) / 16), dim3(8, 8), 0, stream>>>(dpNv12Merged, dadpNv12, nImage, nWidth, nHeight);
    ck(cudaFree(dadpNv12));
}
