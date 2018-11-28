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

#pragma once

#include <inttypes.h>
#include <math.h>

template <typename T>
void SumSquareErrorFor420Planar(T *p0, T *p1, int nWidth, int nHeight, int64_t *py, int64_t *pu, int64_t *pv, int shift) {
    int64_t y = 0, u = 0, v = 0, d;
    for (int i = 0; i < nWidth * nHeight; i++) {
        d = (p0[i] >> shift) - (p1[i] >> shift);
        y += d * d;
    }
    p0 += nWidth * nHeight;
    p1 += nWidth * nHeight;
    for (int y = 0; y < nHeight / 2; y++) {
        for (int x = 0; x < nWidth / 2; x++) {
            d = (p0[y * nWidth / 2 + x] >> shift) - (p1[y * nWidth / 2 + x] >> shift);
            u += d * d;
        }
    }
    p0 += nWidth * nHeight / 4;
    p1 += nWidth * nHeight / 4;
    for (int y = 0; y < nHeight / 2; y++) {
        for (int x = 0; x < nWidth / 2; x++) {
            d = (p0[y * nWidth / 2 + x] >> shift) - (p1[y * nWidth / 2 + x] >> shift);
            v += d * d;
        }
    }
    *py = y;
    *pu = u;
    *pv = v;
}

inline double psnr(int64_t sse, int64_t n, double max) {
    if (sse == 0) return 0;
    return 10.0 * log10(max * max * n / sse);
}
