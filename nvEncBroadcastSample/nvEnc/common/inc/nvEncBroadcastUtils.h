/*
* Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdint.h>
#include <string>

#ifndef INVALID_VALUE
#define INVALID_VALUE 0xFFFFFFFF
#endif

#ifndef MAX_LENGTH
#define MAX_LENGTH  4096
#endif

bool IsWin7OrGreater(void);
bool IsWin7(void);

namespace nvEncBroadcastUtils
{
    //////////////////////////////////////////////////////////////////////////
    //  CritSec
    //  Description: Wraps a critical section.
    //////////////////////////////////////////////////////////////////////////

    class CritSec
    {
    private:
        CRITICAL_SECTION m_criticalSection;
    public:
        CritSec();
        ~CritSec();

        void Lock();
        void Unlock();
    };

    //////////////////////////////////////////////////////////////////////////
    //  AutoLock
    //  Description: Provides automatic locking and unlocking of a 
    //               of a critical section.
    //
    //  Note: The AutoLock object must go out of scope before the CritSec.
    //////////////////////////////////////////////////////////////////////////

    class AutoLock
    {
    private:
        CritSec *m_pCriticalSection;
    public:
        AutoLock(CritSec& crit);

        ~AutoLock();
    };
}

class tictoc
{
private:
    __int64 m_counterStart;
public:
    tictoc();
    void tic();
    void toc(LPCTSTR lpszFormat = NULL, ...);
    void elapsed(LPCTSTR lpszFormat, ...);
    double getelapsed();
};

#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#define SAFE_DELETE(x) { delete x; x = NULL; }
#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }

//dbg Macros
enum DebugLevel
{
    dbgERROR = 0x00000001,
    dbgWARNING = 0x00000002,
    dbgINFO = 0x00000004,
    dbgPROFILE = 0x00000008,
};

#define DEBUG_LEVEL_MASK 0xf
#define DEBUG_FILTER_MASK (~DEBUG_LEVEL_MASK)

void DBGMSG(DebugLevel ulLevel, PCTSTR format, ...);
DebugLevel  getGlobalDbgLevel();

HRESULT DebugLog_Initialize(const WCHAR *sFileName);
void    DebugLog_Trace(DebugLevel ulLevel, const WCHAR *sFormatString, ...);
void    DebugLog_Close();

#define FUNCTIONW(x) L##x
#define FUNCTIONW1(x) FUNCTIONW(x)
#define WFUNCTION FUNCTIONW1(__FUNCTION__)