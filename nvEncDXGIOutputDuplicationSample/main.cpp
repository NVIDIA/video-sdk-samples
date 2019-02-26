/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Defs.h"
#include "DDAImpl.h"
#include "Preproc.h"
#include "NvEncoder/NvEncoderD3D11.h"

class DemoApplication
{
    /// Demo Application Core class
#define returnIfError(x)\
    if (FAILED(x))\
    {\
        printf(__FUNCTION__": Line %d, File %s Returning error 0x%08x\n", __LINE__, __FILE__, x);\
        return x;\
    }

private:
    /// DDA wrapper object, defined in DDAImpl.h
    DDAImpl *pDDAWrapper = nullptr;
    /// PreProcesor for encoding. Defined in Preproc.h
    /// Preprocessingis required if captured images are of different size than encWidthxencHeight
    /// This application always uses this preprocessor
    RGBToNV12 *pColorConv = nullptr;
    /// NVENCODE API wrapper. Defined in NvEncoderD3D11.h. This class is imported from NVIDIA Video SDK
    NvEncoderD3D11 *pEnc = nullptr;
    /// D3D11 device context used for the operations demonstrated in this application
    ID3D11Device *pD3DDev = nullptr;
    /// D3D11 device context
    ID3D11DeviceContext *pCtx = nullptr;
    /// D3D11 RGB Texture2D object that recieves the captured image from DDA
    ID3D11Texture2D *pDupTex2D = nullptr;
    /// D3D11 YUV420 Texture2D object that sends the image to NVENC for video encoding
    ID3D11Texture2D *pEncBuf = nullptr;
    /// Output video bitstream file handle
    FILE *fp = nullptr;
    /// Failure count from Capture API
    UINT failCount = 0;
    /// Video output dimensions
    const static UINT encWidth = 1920;
    const static UINT encHeight = 1080;
    /// turn off preproc, let NVENCODE API handle colorspace conversion
    const static bool bNoVPBlt = false;
    /// Video output file name
    const char fnameBase[64] = "DDATest_%d.h264";
    /// Encoded video bitstream packet in CPU memory
    std::vector<std::vector<uint8_t>> vPacket;
    /// NVENCODEAPI session intialization parameters
    NV_ENC_INITIALIZE_PARAMS encInitParams = { 0 };
    /// NVENCODEAPI video encoding configuration parameters
    NV_ENC_CONFIG encConfig = { 0 };
private:
    /// Initialize DXGI pipeline
    HRESULT InitDXGI()
    {
        HRESULT hr = S_OK;
        /// Driver types supported
        D3D_DRIVER_TYPE DriverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };
        UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

        /// Feature levels supported
        D3D_FEATURE_LEVEL FeatureLevels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1
        };
        UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
        D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

        /// Create device
        for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
        {
            hr = D3D11CreateDevice(nullptr, DriverTypes[DriverTypeIndex], nullptr, /*D3D11_CREATE_DEVICE_DEBUG*/0, FeatureLevels, NumFeatureLevels,
                D3D11_SDK_VERSION, &pD3DDev, &FeatureLevel, &pCtx);
            if (SUCCEEDED(hr))
            {
                // Device creation succeeded, no need to loop anymore
                break;
            }
        }
        return hr;
    }

    /// Initialize DDA handler
    HRESULT InitDup()
    {
        HRESULT hr = S_OK;
        if (!pDDAWrapper)
        {
            pDDAWrapper = new DDAImpl(pD3DDev, pCtx);
            hr = pDDAWrapper->Init();
            returnIfError(hr);
        }
        return hr;
    }

    /// Initialize NVENCODEAPI wrapper
    HRESULT InitEnc()
    {
        if (!pEnc)
        {
            DWORD w = bNoVPBlt ? pDDAWrapper->getWidth() : encWidth; 
            DWORD h = bNoVPBlt ? pDDAWrapper->getHeight() : encHeight;
            NV_ENC_BUFFER_FORMAT fmt = bNoVPBlt ? NV_ENC_BUFFER_FORMAT_ARGB : NV_ENC_BUFFER_FORMAT_NV12;
            pEnc = new NvEncoderD3D11(pD3DDev, w, h, fmt);
            if (!pEnc)
            {
                returnIfError(E_FAIL);
            }

            ZeroMemory(&encInitParams, sizeof(encInitParams));
            ZeroMemory(&encConfig, sizeof(encConfig));
            encInitParams.encodeConfig = &encConfig;
            encInitParams.encodeWidth = w;
            encInitParams.encodeHeight = h;
            encInitParams.maxEncodeWidth = pDDAWrapper->getWidth();
            encInitParams.maxEncodeHeight = pDDAWrapper->getHeight();
            encConfig.gopLength = 5;

            try
            {
                pEnc->CreateDefaultEncoderParams(&encInitParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_LOW_LATENCY_HP_GUID);
                pEnc->CreateEncoder(&encInitParams);
            }
            catch (...)
            {
                returnIfError(E_FAIL);
            }
        }
        return S_OK;
    }

    /// Initialize preprocessor
    HRESULT InitColorConv()
    {
        if (!pColorConv)
        {
            pColorConv = new RGBToNV12(pD3DDev, pCtx);
            HRESULT hr = pColorConv->Init();
            returnIfError(hr);
        }
        return S_OK;
    }

    /// Initialize Video output file
    HRESULT InitOutFile()
    {
        if (!fp)
        {
            char fname[64] = { 0 };
            sprintf_s(fname, (const char *)fnameBase, failCount);
            errno_t err = fopen_s(&fp, fname, "wb");
            returnIfError(err);
        }
        return S_OK;
    }

    /// Write encoded video output to file
    void WriteEncOutput()
    {
        int nFrame = 0;
        nFrame = (int)vPacket.size();
        for (std::vector<uint8_t> &packet : vPacket)
        {
            fwrite(packet.data(), packet.size(), 1, fp);
        }
    }

public:
    /// Initialize demo application
    HRESULT Init()
    {
        HRESULT hr = S_OK;

        hr = InitDXGI();
        returnIfError(hr);

        hr = InitDup();
        returnIfError(hr);


        hr = InitEnc();
        returnIfError(hr);

        hr = InitColorConv();
        returnIfError(hr);

        hr = InitOutFile();
        returnIfError(hr);
        return hr;
    }

    /// Capture a frame using DDA
    HRESULT Capture(int wait)
    {
        HRESULT hr = pDDAWrapper->GetCapturedFrame(&pDupTex2D, wait); // Release after preproc
        if (FAILED(hr))
        {
            failCount++;
        }
        return hr;
    }

    /// Preprocess captured frame
    HRESULT Preproc()
    {
        HRESULT hr = S_OK;
        const NvEncInputFrame *pEncInput = pEnc->GetNextInputFrame();
        pEncBuf = (ID3D11Texture2D *)pEncInput->inputPtr;
        if (bNoVPBlt)
        {
            pCtx->CopySubresourceRegion(pEncBuf, D3D11CalcSubresource(0, 0, 1), 0, 0, 0, pDupTex2D, 0, NULL);
        }
        else
        {
            hr = pColorConv->Convert(pDupTex2D, pEncBuf);
        }
        SAFE_RELEASE(pDupTex2D);
        returnIfError(hr);

        pEncBuf->AddRef();  // Release after encode
        return hr;
    }

    /// Encode the captured frame using NVENCODEAPI
    HRESULT Encode()
    {
        HRESULT hr = S_OK;
        try
        {
            pEnc->EncodeFrame(vPacket);
            WriteEncOutput();
        }
        catch (...)
        {
            hr = E_FAIL;
        }
        SAFE_RELEASE(pEncBuf);
        return hr;
    }

    /// Release all resources
    void Cleanup(bool bDelete = true)
    {
        if (pDDAWrapper)
        {
            pDDAWrapper->Cleanup();
            delete pDDAWrapper;
            pDDAWrapper = nullptr;
        }

        if (pColorConv)
        {
            pColorConv->Cleanup();
        }

        if (pEnc)
        {
            ZeroMemory(&encInitParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
            ZeroMemory(&encConfig, sizeof(NV_ENC_CONFIG));
        }

        SAFE_RELEASE(pDupTex2D);
        if (bDelete)
        {
            if (pEnc)
            {
                /// Flush the encoder and write all output to file before destroying the encoder
                pEnc->EndEncode(vPacket);
                WriteEncOutput();
                pEnc->DestroyEncoder();
                if (bDelete)
                {
                    delete pEnc;
                    pEnc = nullptr;
                }

                ZeroMemory(&encInitParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
                ZeroMemory(&encConfig, sizeof(NV_ENC_CONFIG));
            }

            if (pColorConv)
            {
                delete pColorConv;
                pColorConv = nullptr;
            }
            SAFE_RELEASE(pD3DDev);
            SAFE_RELEASE(pCtx);
        }
    }
    DemoApplication() {}
    ~DemoApplication()
    {
        Cleanup(true); 
    }
};

/// Demo 60 FPS (approx.) capture
int Grab60FPS(int nFrames)
{
    const int WAIT_BASE = 17;
    DemoApplication Demo;
    HRESULT hr = S_OK;
    int capturedFrames = 0;
    LARGE_INTEGER start = { 0 };
    LARGE_INTEGER end = { 0 };
    LARGE_INTEGER interval = { 0 };
    LARGE_INTEGER freq = { 0 };
    int wait = WAIT_BASE;

    QueryPerformanceFrequency(&freq);

    /// Reset waiting time for the next screen capture attempt
#define RESET_WAIT_TIME(start, end, interval, freq)         \
    QueryPerformanceCounter(&end);                          \
    interval.QuadPart = end.QuadPart - start.QuadPart;      \
    MICROSEC_TIME(interval, freq);                          \
    wait = (int)(WAIT_BASE - (interval.QuadPart * 1000));

    /// Initialize Demo app
    hr = Demo.Init();
    if (FAILED(hr))
    {
        printf("Initialization failed with error 0x%08x\n", hr);
        return -1;
    }

    /// Run capture loop
    do
    {
        /// get start timestamp. 
        /// use this to adjust the waiting period in each capture attempt to approximately attempt 60 captures in a second
        QueryPerformanceCounter(&start);
        /// Get a frame from DDA
        hr = Demo.Capture(wait);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) 
        {
            /// retry if there was no new update to the screen during our specific timeout interval
            /// reset our waiting time
            RESET_WAIT_TIME(start, end, interval, freq);
            continue;
        }
        else
        {
            if (FAILED(hr))
            {
                /// Re-try with a new DDA object
                printf("Captrue failed with error 0x%08x. Re-create DDA and try again.\n", hr);
                Demo.Cleanup();
                hr = Demo.Init();
                if (FAILED(hr))
                {
                    /// Could not initialize DDA, bail out/
                    printf("Failed to Init DDDemo. return error 0x%08x\n", hr);
                    return -1;
                }
                RESET_WAIT_TIME(start, end, interval, freq);
                QueryPerformanceCounter(&start);
                /// Get a frame from DDA
                Demo.Capture(wait);
            }
            RESET_WAIT_TIME(start, end, interval, freq);
            /// Preprocess for encoding
            hr = Demo.Preproc(); 
            if (FAILED(hr))
            {
                printf("Preproc failed with error 0x%08x\n", hr);
                return -1;
            }
            hr = Demo.Encode();
            if (FAILED(hr))
            {
                printf("Encode failed with error 0x%08x\n", hr);
                return -1;
            }
            capturedFrames++;
        }
    } while (capturedFrames <= nFrames);

    return 0;
}

void printHelp()
{
    printf(" DXGIOUTPUTDuplication_NVENC_Demo: This application demonstrates using DDA (a.k.a. DesktopDuplication or OutputDuplication or IDXGIOutputDuplication API\n\
                                               to capture desktop and efficiently encode the captured images using NVENCODEAPI.\n\
                                               The output video bitstream will be generated with name DDATest_0.h264 in current working directory.\n\
                                               This application only captures the primary desktop.\n");
    printf(" DXGIOUTPUTDuplication_NVENC_Demo: Commandline help\n");
    printf(" DXGIOUTPUTDuplication_NVENC_Demo: '-?', '?', '-about', 'about', '-h', '-help', '-usage', 'h', 'help', 'usage': Print this Commandline help\n");
    printf(" DXGIOUTPUTDuplication_NVENC_Demo: '-frames <n>': n = No. of frames to capture");
}

int main(int argc, char** argv)
{
    /// The app will try to capture 60 times, by default
    int nFrames = 60;
    int ret = 0;
    bool useNvenc = true;

    /// Parse arguments
    try
    {
        if (argc > 1)
        {
            for (int i = 0; i < argc; i++)
            {
                if (!strcmpi("-frames", argv[i]))
                {
                    nFrames = atoi(argv[i + 1]);
                }
                else if (!strcmpi("-frames", argv[i]))
                {
                    useNvenc = true;
                }
                else if ((!strcmpi("-help", argv[i])) ||
                         (!strcmpi("-h", argv[i])) ||
                         (!strcmpi("h", argv[i])) ||
                         (!strcmpi("help", argv[i])) ||
                         (!strcmpi("-usage", argv[i])) ||
                         (!strcmpi("-about", argv[i])) ||
                         (!strcmpi("-?", argv[i])) ||
                         (!strcmpi("?", argv[i])) ||
                         (!strcmpi("about", argv[i])) ||
                         (!strcmpi("usage", argv[i]))
                        )
                {
                    printHelp();
                }
            }
        }
        else
        {
            printHelp();
            return 0;
        }
    }
    catch (...)
    {
        printf(" DXGIOUTPUTDuplication_NVENC_Demo: Invalid arguments passed.\n\
                                                   Continuing to grab 60 frames.\n");
        printHelp();
    }
    printf(" DXGIOUTPUTDuplication_NVENC_Demo: Frames to Capture: %d.\n", nFrames);

    /// Kick off the demo
    ret = Grab60FPS(nFrames);
    return ret;
}