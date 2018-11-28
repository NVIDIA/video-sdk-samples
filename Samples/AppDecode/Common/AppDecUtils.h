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
#include <sstream>
#include <iostream>

void ShowHelpAndExit(const char *szBadOption, char *szOutputFileName, bool *pbVerbose, int *piD3d)
{
    std::ostringstream oss;
    bool bThrowError = false;
    if (szBadOption)
    {
        bThrowError = false;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    std::cout << "Options:" << std::endl
        << "-i           Input file path" << std::endl
        << (szOutputFileName ? "-o           Output file path\n" : "")
        << "-gpu         Ordinal of GPU to use" << std::endl
        << (pbVerbose        ? "-v           Verbose message\n" : "")
        << (piD3d            ? "-d3d         9 (default): display with D3D9; 11: display with D3D11\n" : "")
        ;
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

void ParseCommandLine(int argc, char *argv[], char *szInputFileName,
    char *szOutputFileName, int &iGpu, bool *pbVerbose = NULL, int *piD3d = NULL) 
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++) {
        if (!_stricmp(argv[i], "-h")) {
            ShowHelpAndExit(NULL, szOutputFileName, pbVerbose, piD3d);
        }
        if (!_stricmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit("-i", szOutputFileName, pbVerbose, piD3d);
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o")) {
            if (++i == argc || !szOutputFileName) {
                ShowHelpAndExit("-o", szOutputFileName, pbVerbose, piD3d);
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu", szOutputFileName, pbVerbose, piD3d);
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-v")) {
            if (!pbVerbose) {
                ShowHelpAndExit("-v", szOutputFileName, pbVerbose, piD3d);
            }
            *pbVerbose = true;
            continue;
        }
        if (!_stricmp(argv[i], "-d3d")) {
            if (++i == argc || !piD3d) {
                ShowHelpAndExit("-d3d", szOutputFileName, pbVerbose, piD3d);
            }
            *piD3d = atoi(argv[i]);
            continue;
        }
        ShowHelpAndExit(argv[i], szOutputFileName, pbVerbose, piD3d);
    }
}
