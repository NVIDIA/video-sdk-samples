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
#include <memory>
#include <stdint.h>
#include "../Utils/Logger.h"
#include "NvEncoder/NvEncoderGL.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/freeglut_ext.h>


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
        << "-if          Input format: iyuv nv12" << std::endl
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
    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam)
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

        std::vector<std::string> vszFileFormatName = { "iyuv", "nv12" };

        NV_ENC_BUFFER_FORMAT aFormat[] =
        {
            NV_ENC_BUFFER_FORMAT_IYUV,
            NV_ENC_BUFFER_FORMAT_NV12,
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

void EncodeGL(char *szInFilePath, char *szOutFilePath, int nWidth, int nHeight,
    NV_ENC_BUFFER_FORMAT eFormat, NvEncoderInitParam *encodeCLIOptions)
{
    std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpIn)
    {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    NvEncoderGL enc(nWidth, nHeight, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions->GetEncodeGUID(),
        encodeCLIOptions->GetPresetGUID());

    encodeCLIOptions->SetInitParams(&initializeParams, eFormat);

    enc.CreateEncoder(&initializeParams);

    int nFrameSize = enc.GetFrameSize();
    std::unique_ptr<uint8_t[]> pHostFrame(new uint8_t[nFrameSize]);
    int nFrame = 0;
    while (true)
    {
        std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(pHostFrame.get()), nFrameSize).gcount();

        const NvEncInputFrame* encoderInputFrame = enc.GetNextInputFrame();
        NV_ENC_INPUT_RESOURCE_OPENGL_TEX *pResource = (NV_ENC_INPUT_RESOURCE_OPENGL_TEX *)encoderInputFrame->inputPtr;

        glBindTexture(pResource->target, pResource->texture);
        glTexSubImage2D(pResource->target, 0, 0, 0,
            nWidth, nHeight * 3 / 2,
            GL_RED, GL_UNSIGNED_BYTE, pHostFrame.get());
        glBindTexture(pResource->target, 0);

        std::vector<std::vector<uint8_t>> vPacket;
        if (nRead == nFrameSize)
        {
            enc.EncodeFrame(vPacket);
        }
        else
        {
            enc.EndEncode(vPacket);
        }
        nFrame += (int)vPacket.size();
        for (std::vector<uint8_t> &packet : vPacket)
        {
            fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
        if (nRead != nFrameSize) break;
    }

    enc.DestroyEncoder();

    fpOut.close();
    fpIn.close();

    std::cout << "Total frames encoded: " << nFrame << std::endl;
    std::cout << "Saved in file " << szOutFilePath << std::endl;
}

/**
*  This sample application illustrates encoding of frames stored in OpenGL
*  textures. The application reads frames from the input file and uploads them
*  to the textures obtained from the encoder using NvEncoder::GetNextInputFrame().
*  The encoder subsequently maps the textures for encoder using NvEncodeAPI and
*  submits them to NVENC hardware for encoding as part of EncodeFrame() function.
*/

int main(int argc, char **argv)
{
    char szInFilePath[256] = "",
        szOutFilePath[256] = "";
    int nWidth = 1920, nHeight = 1080;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    NvEncoderInitParam encodeCLIOptions;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutInitWindowSize(16, 16);
    int window = glutCreateWindow("AppEncGL");
    if (!window)
    {
        std::cout << "Unable to create GLUT window." << std::endl;
        return 1;
    }
    glutHideWindow();

    try
    {
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eFormat,
            szOutFilePath, encodeCLIOptions);

        CheckInputFile(szInFilePath);

        if (!*szOutFilePath)
        {
            sprintf(szOutFilePath, encodeCLIOptions.IsCodecH264() ? "out.h264" : "out.hevc");
        }

        EncodeGL(szInFilePath, szOutFilePath, nWidth, nHeight, eFormat, &encodeCLIOptions);
    }
    catch (const std::exception &e)
    {
        std::cout << e.what();
        return 1;
    }

    glutDestroyWindow(window);

    return 0;
}
