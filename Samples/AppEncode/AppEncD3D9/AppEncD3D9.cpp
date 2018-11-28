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

#define INITGUID

#include <d3d9.h>
#include <dxva2api.h>
#include <iostream>
#include <memory>
#include <wrl.h>
#include "NvEncoder/NvEncoderD3D9.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/Logger.h"
#include "../Common/AppEncUtils.h"

using Microsoft::WRL::ComPtr;


simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

const D3DFORMAT D3DFMT_NV12 = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');

class RGBToNV12ConverterD3D9 {
public:
    RGBToNV12ConverterD3D9(IDirect3DDevice9 *pDevice, IDirect3DSurface9* pSurfaceBgra) : pD3D9Device(pDevice)
    {
        pD3D9Device->AddRef();
        D3DSURFACE_DESC desc;
        ck(pSurfaceBgra->GetDesc(&desc));
        ck(DXVA2CreateVideoService(pDevice, __uuidof(IDirectXVideoProcessorService), (void **)&pService));
        ck(pService->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice, &videoDesc, D3DFMT_NV12, 0, &pProcessor));

        RECT rect = {0, 0, (long)desc.Width, (long)desc.Height};
        videoSample.PlanarAlpha.ll = 0x10000;
        videoSample.SrcSurface = pSurfaceBgra;
        videoSample.SrcRect = rect;
        videoSample.DstRect = rect;
        videoSample.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
        videoSample.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
        videoSample.SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
        videoSample.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;

        bltParam.TargetRect = rect;
        bltParam.DestFormat = videoSample.SampleFormat;
        bltParam.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
        bltParam.Alpha.ll = 0x10000;
        bltParam.TargetFrame = videoSample.Start;
        bltParam.BackgroundColor.Y = 0x1000;
        bltParam.BackgroundColor.Cb = 0x8000;
        bltParam.BackgroundColor.Cr = 0x8000;
        bltParam.BackgroundColor.Alpha = 0xffff;
        DXVA2_ValueRange vr;
        ck(pService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &videoDesc, D3DFMT_NV12, DXVA2_ProcAmp_Brightness, &vr));
        bltParam.ProcAmpValues.Brightness = vr.DefaultValue;
        ck(pService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &videoDesc, D3DFMT_NV12, DXVA2_ProcAmp_Contrast, &vr));
        bltParam.ProcAmpValues.Contrast = vr.DefaultValue;
        ck(pService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &videoDesc, D3DFMT_NV12, DXVA2_ProcAmp_Hue, &vr));
        bltParam.ProcAmpValues.Hue = vr.DefaultValue;
        ck(pService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &videoDesc, D3DFMT_NV12, DXVA2_ProcAmp_Saturation, &vr));
        bltParam.ProcAmpValues.Saturation = vr.DefaultValue;
    }
    ~RGBToNV12ConverterD3D9() 
    {
        if (pProcessor) pProcessor->Release();
        if (pService) pService->Release();
        if (pD3D9Device) pD3D9Device->Release();
    }

    void ConvertRGBToNV12(IDirect3DSurface9* pDestNV12Surface)
    {
        ck(pProcessor->VideoProcessBlt(pDestNV12Surface, &bltParam, &videoSample, 1, NULL));
        return;
    }
    IDirectXVideoProcessorService* GetVideoProcessService() { return pService; }
private:
    IDirectXVideoProcessorService *pService = NULL;
    IDirectXVideoProcessor *pProcessor = NULL;
    DXVA2_VideoDesc videoDesc = {};
    DXVA2_VideoSample videoSample = {};
    DXVA2_VideoProcessBltParams bltParam = {};
    IDirect3DDevice9 *pD3D9Device = NULL;

};

void Encode(char *szBgraFilePath, int nWidth, int nHeight, char *szOutFilePath, NvEncoderInitParam *pEncodeCLIOptions,
    int iGpu, bool bForceNv12)
{
    ComPtr<IDirect3DDevice9> pDevice;
    ComPtr<IDirect3D9Ex> pD3D;
    ComPtr<IDirect3DSurface9> pSurfaceBgra;

    ck(Direct3DCreate9Ex(D3D_SDK_VERSION, pD3D.GetAddressOf()));

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.BackBufferWidth = nWidth;
    d3dpp.BackBufferHeight = nHeight;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.Windowed = TRUE;
    d3dpp.hDeviceWindow = NULL;
    ck(pD3D->CreateDevice(iGpu, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, pDevice.GetAddressOf()));
    D3DADAPTER_IDENTIFIER9 id;
    pD3D->GetAdapterIdentifier(iGpu, 0, &id);
    std::cout << "GPU in use: " << id.Description << std::endl;

    ck(pDevice->CreateOffscreenPlainSurface(nWidth, nHeight, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, pSurfaceBgra.GetAddressOf(), NULL));

    std::unique_ptr<RGBToNV12ConverterD3D9> pConverter;
    if (bForceNv12)
    {
        pConverter.reset(new RGBToNV12ConverterD3D9(pDevice.Get(), pSurfaceBgra.Get()));
    }

    NvEncoderD3D9 enc(pDevice.Get(), nWidth, nHeight, bForceNv12 ? NV_ENC_BUFFER_FORMAT_NV12 : NV_ENC_BUFFER_FORMAT_ARGB, bForceNv12 ? pConverter->GetVideoProcessService() : nullptr);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, pEncodeCLIOptions->GetEncodeGUID(), pEncodeCLIOptions->GetPresetGUID());

    pEncodeCLIOptions->SetInitParams(&initializeParams, bForceNv12 ? NV_ENC_BUFFER_FORMAT_NV12 : NV_ENC_BUFFER_FORMAT_ARGB);

    enc.CreateEncoder(&initializeParams);

    std::ifstream fpBgra(szBgraFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpBgra)
    {
        std::ostringstream err;
        err << "Unable to open input file: " << szBgraFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    int nSize = nWidth * nHeight * 4;
    std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nSize]);

    int nFrame = 0;
    while (true) 
    {
        std::vector<std::vector<uint8_t>> vPacket;
        std::streamsize nRead = fpBgra.read(reinterpret_cast<char*>(pHostFrame.get()), nSize).gcount();
        if (nRead == nSize)
        {
            const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();
            IDirect3DSurface9 *pEncodeInputSurface = reinterpret_cast<IDirect3DSurface9*>(encoderInputFrame->inputPtr);
            IDirect3DSurface9* pRGBSurface = bForceNv12 ? pSurfaceBgra.Get() : pEncodeInputSurface;
            // Load frame into pSurfaceBgra
            D3DLOCKED_RECT lockedRect;
            ck(pRGBSurface->LockRect(&lockedRect, NULL, 0));
            for (int y = 0; y < nHeight; y++) 
            {
                memcpy((uint8_t *)lockedRect.pBits + y * lockedRect.Pitch, pHostFrame.get() + y * nWidth * 4, nWidth * 4);
            }
            ck(pRGBSurface->UnlockRect());
            if (bForceNv12)
            {
                pConverter->ConvertRGBToNV12(pEncodeInputSurface);
            }
            enc.EncodeFrame(vPacket);
        } 
        else
        {
            enc.EndEncode(vPacket);
        }
        nFrame += (int)vPacket.size();;
        for (std::vector<uint8_t> &packet : vPacket) {
            fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
        if (nRead != nSize) {
            break;
        }
    }

    enc.DestroyEncoder();

    fpOut.close();
    fpBgra.close();

    std::cout << "Total frames encoded: " << nFrame << std::endl << "Saved in file " << szOutFilePath << std::endl;
}

/**
*  This sample application illustrates encoding of frames in IDirect3DSurface9 surfaces.
*  There are 2 modes of operation demonstrated in this application.
*  In the default mode application reads RGB data from file and copies it to D3D9 surfaces
*  obtained from the encoder using NvEncoder::GetNextInputFrame() and the RGB surface is
*  submitted to NVENC for encoding. In the second case ("-nv12" option) the application performs a
*  color space conversion from RGB to NV12 using DXVA's VideoProcessBlt API call and the NV12
*  surface is submitted for encoding.
*/
int main(int argc, char **argv)
{
    char szInFilePath[256] = "";
    char szOutFilePath[256] = "out.h264";
    int nWidth = 1920, nHeight = 1080; 
    NvEncoderInitParam encodeCLIOptions;
    int iGpu = 0;
    bool bForceNv12 = false;
    try
    {
        ParseCommandLine_AppEncD3D(argc, argv, szInFilePath, nWidth, nHeight, szOutFilePath, encodeCLIOptions, iGpu, bForceNv12);

        CheckInputFile(szInFilePath);

        Encode(szInFilePath, nWidth, nHeight, szOutFilePath, &encodeCLIOptions, iGpu, bForceNv12);
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
