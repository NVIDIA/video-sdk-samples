/*
* Copyright 2018 NVIDIA Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy of this
* software and associated documentation files (the "Software"),  to deal in the Software
* without restriction, including without limitation the rights to use, copy, modify,
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to the following conditions:
* The above copyright notice and this permission notice shall be included in all copies
* or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
* PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
* OR OTHER DEALINGS IN THE SOFTWARE.
*
*/

#include <windows.h>
#include <iostream>
#include <fstream>
#include "nvEncApp.h"

using namespace nvEncBroadcastApi;
using namespace Microsoft::WRL;

namespace
{
    AppEncodeParams      g_Params;
    //-------------------------------------------------------------------
    // InitializeParams
    //-------------------------------------------------------------------
    void InitializeParams()
    {
        memset(&g_Params, 0, sizeof(g_Params));
        g_Params.width = DEFAULT_WIDTH;
        g_Params.height = DEFAULT_HEIGHT;
    } //InitializeParams
} //namespace

//-------------------------------------------------------------------
// parseCmdLine
//-------------------------------------------------------------------
bool parseCmdLine(int argc, char **argv)
{
    std::cout << "Printing arguments" << std::endl;
    for (int i = 0; i < argc; i++)
    {
        std::cout << argv[i] << std::endl;
    }

    for (int cnt = 1; cnt < argc; ++cnt)
    {
        if (0 == _stricmp(argv[cnt], "-input"))
        {
            ++cnt;
            if (cnt >= argc)
            {
                std::cout << "Missing arguements for -input option" << std::endl;
                return false;
            }
            mbstowcs(g_Params.inputFile, argv[cnt], MAX_LENGTH - 1);
            if (!PathFileExists(g_Params.inputFile))
            {
                std::cout << "File - " << argv[cnt] << " doesnt exist" << std::endl;
                return false;
            }
        }
        else if (0 == _stricmp(argv[cnt], "-output"))
        {
            ++cnt;
            if (cnt >= argc)
            {
                std::cout << "Missing arguements for -output option" << std::endl;
                return false;
            }
            mbstowcs(g_Params.outputFile, argv[cnt], MAX_LENGTH - 1);
        }
        else if (0 == _stricmp(argv[cnt], "-width"))
        {
            ++cnt;
            if (cnt >= argc)
            {
                std::cout << "Missing arguements for -width option" << std::endl;
                return false;
            }
            unsigned int width;
            sscanf(argv[cnt], "%d", &width);
            g_Params.width = width;
        }
        else if (0 == _stricmp(argv[cnt], "-height"))
        {
            ++cnt;
            if (cnt >= argc)
            {
                std::cout << "Missing arguements for -height option" << std::endl;
                return false;
            }
            unsigned int height;
            sscanf(argv[cnt], "%d", &height);
            g_Params.height = height;        }
    }
    return wcslen(g_Params.inputFile) != 0 && wcslen(g_Params.outputFile) != 0;
} //parseCmdLine

//-------------------------------------------------------------------
// main
//-------------------------------------------------------------------
int main(int argc, char* argv[])
{
    std::ifstream fpFile;
    std::ofstream fpOut;
    INVENC_EncodeBitstreamBuffer* pBuffer = nullptr;
    INVENC_EncodeBitstreamBuffer* pSequenceBuffer = nullptr;

    InitializeParams();

    if (!parseCmdLine(argc, argv)) {
        std::cerr << "Error with arguments...exiting" << std::endl;
        std::cout << "Usage: nvEncodeapp.exe -input <filename> -output <filename> -width <width> -height <height>" << std::endl;
        getchar();
        return -1;
    }

    nvEncApp nvApp;

    NVENC_EncodeCreateParams createParams;
    memset(&createParams, 0, sizeof(NVENC_EncodeCreateParams));
    uint32_t nSize = 0;
    eNVENC_RetCode retCode = API_SUCCESS;
    NVENC_EncodeInitParams initParams = { 0 };
    std::unique_ptr<uint8_t[]> pHostFrame;

    fpFile = std::ifstream(g_Params.inputFile, std::ifstream::in | std::ifstream::binary);
    if (!fpFile.is_open())
    {
        std::cerr << "Not able to open input file...exiting" << std::endl;
        goto exit;
    }

    fpOut = std::ofstream(g_Params.outputFile, std::ios::out | std::ios::binary);
    if (!fpOut.is_open())
    {
        std::cerr << "Not able to open output file...exiting" << std::endl;
        goto exit;
    }

    bool bRet = nvApp.InitializeNvEncContext(g_Params, initParams, nSize);
    if (!bRet)
    {
        std::cerr << "Not able to initialize nvEnc Context...exiting" << std::endl;
        goto exit;
    }

    INVENC_EncodeApiObj* pEncodeObj = nvApp.GetNvEncObj();
    if (!pEncodeObj)
    {
        std::cerr << "No valid nvEnc Api Context retrieved...exiting" << std::endl;
        goto exit;
    }

    retCode = pEncodeObj->createBitstreamBuffer(&pBuffer);
    if (FAILED_NVENC_BROADCASTAPI(retCode) || !pBuffer)
    {
        std::cerr << "Not able to create bitstream buffer object...exiting" << std::endl;
        goto exit;
    }

    retCode = pEncodeObj->createBitstreamBuffer(&pSequenceBuffer);
    if (FAILED_NVENC_BROADCASTAPI(retCode) || !pSequenceBuffer)
    {
        std::cerr << "Not able to create bitstream buffer object for sequence params...exiting" << std::endl;
        goto exit;
    }

    pHostFrame.reset((new uint8_t[nSize]));
    NVENC_EncodeInfo encodeInfo;
    memset(&encodeInfo, 0, sizeof(encodeInfo));
    HRESULT hr = S_OK;
    if (pHostFrame.get())
    {
        int nFrame = 0;
        while (true)
        {
            std::streamsize nRead = fpFile.read(reinterpret_cast<char*>(pHostFrame.get()), nSize).gcount();
            if (nRead == nSize)
            {
                bool bRet = nvApp.SetFrameParams(encodeInfo, initParams, pHostFrame.get());
                if (bRet)
                {
                    if (nRead == nSize)
                    {
                        pEncodeObj->encode(&encodeInfo, pBuffer);
                    }
                    else
                    {
                        pEncodeObj->finalize(pBuffer);
                    }
                }
            }
            else
            {
                pEncodeObj->finalize(pBuffer);
            }
            uint32_t size;
            uint8_t* pBitStream = pBuffer->getBitStreamBuffer(&size);
            if (pBitStream && size)
            {
                //if first frame, get the SPPPS via sequence params
                if (g_Params.numFrames == 0)
                {
                    pEncodeObj->getSequenceParams(pSequenceBuffer);
                }
                if (fpOut.is_open())
                {
                    fpOut.write(reinterpret_cast<char*>(pBitStream), size);
                }
                ++g_Params.numFrames;
                pBuffer->recycle();
            }
            if (nRead != nSize)
            {
                std::cerr << "Data from input file either read completely or not enough for a frame...exiting" << std::endl;
                break;
            }
        }
    }
exit:
    if (pSequenceBuffer)
    {
        pSequenceBuffer->release();
        pSequenceBuffer = nullptr;
    }

    if (pBuffer)
    {
        pBuffer->release();
        pBuffer = nullptr;
    }

    if (fpFile.is_open())
    {
        fpFile.close();
    }

    if (fpOut.is_open())
    {
        fpOut.close();
    }
} //main