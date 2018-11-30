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

#include <windows.h>
#pragma warning( push )  
#pragma warning( disable : 4091 )   // FOR - warning C4091: 'typedef ':ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <ShlObj.h>
#pragma warning( pop )   
#include <sstream>
#include "nvEncBroadcastUtils.h"

namespace nvEncBroadcastUtils
{
    CritSec::CritSec()
    {
        InitializeCriticalSection(&m_criticalSection);
    }

    CritSec::~CritSec()
    {
        DeleteCriticalSection(&m_criticalSection);
    }

    void CritSec::Lock()
    {
        EnterCriticalSection(&m_criticalSection);
    }

    void CritSec::Unlock()
    {
        LeaveCriticalSection(&m_criticalSection);
    }


    //////////////////////////////////////////////////////////////////////////
    //  AutoLock
    //  Description: Provides automatic locking and unlocking of a 
    //               of a critical section.
    //
    //  Note: The AutoLock object must go out of scope before the CritSec.
    //////////////////////////////////////////////////////////////////////////


    AutoLock::AutoLock(CritSec& crit)
    {
        m_pCriticalSection = &crit;
        m_pCriticalSection->Lock();
    }
    AutoLock::~AutoLock()
    {
        m_pCriticalSection->Unlock();
    }
}

// Debug functions
// Log file
FILE* g_hFile = NULL;

//--------------------------------------------------------------------------------------
// Name: Initialize
// Description: Opens a logging file with the specified file name.
//--------------------------------------------------------------------------------------

HRESULT DebugLog_Initialize(const TCHAR *sFileName)
{
    // Close any existing file.
    if (g_hFile != NULL)
    {
        fclose(g_hFile);
        g_hFile = NULL;
    }

    // Open the new logging file.
    if (sFileName)
    {
        g_hFile = _wfsopen(sFileName, L"w", _SH_DENYWR);
        if (g_hFile == NULL)
        {
            OutputDebugString(L"_wfsopen failed for debug log");
            return E_FAIL;
        }
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DebugLog_Trace
// Description: Writes a sprintf-formatted string to the logging file.
//--------------------------------------------------------------------------------------
void DebugLog_Trace(DebugLevel ulLevel, const WCHAR *sFormatString, ...)
{
    HRESULT hr = S_OK;
    if (ulLevel <= getGlobalDbgLevel())
    {
        va_list va;
        WCHAR message[MAX_LENGTH];

        va_start(va, sFormatString);
        hr = StringCchVPrintf((STRSAFE_LPWSTR)message, MAX_LENGTH-1, (STRSAFE_LPWSTR)sFormatString, va);
        va_end(va);

        if (!SUCCEEDED(hr))
        {
            fputws(L"StringCchVPrintf possibly truncated\n", g_hFile);
        }
        SYSTEMTIME st;
        GetSystemTime(&st);
        SYSTEMTIME lt;
        SystemTimeToTzSpecificLocalTime(nullptr, &st, &lt);

        fwprintf(
            g_hFile,
            L"%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03hu[%d]: %s\n",
            lt.wYear,
            lt.wMonth,
            lt.wDay,
            lt.wHour,
            lt.wMinute,
            lt.wSecond,
            lt.wMilliseconds,
            ulLevel,
            message);
        fflush(g_hFile);
    }
}

//--------------------------------------------------------------------------------------
// Name: Close
// Description: Closes the logging file and reports any memory leaks.
//--------------------------------------------------------------------------------------
void DebugLog_Close()
{
    if (g_hFile != NULL)
    {
        fclose(g_hFile);
        g_hFile = NULL;
    }
}

void DBGMSG(DebugLevel ulLevel, PCTSTR format, ...)
{
    if (ulLevel <= getGlobalDbgLevel())
    {
        TCHAR msg[MAX_PATH];
        va_list args;
        va_start(args, format);
        StringCbVPrintf(msg, sizeof(msg), format, args);
        va_end(args);

        if (!g_hFile)
        {
            OutputDebugString(msg);
        }
        else
        {
            DebugLog_Trace(ulLevel, msg);
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: tictoc
// Description: Quick timer implementation for profiling
//--------------------------------------------------------------------------------------
tictoc::tictoc()
{
    tic(); // initialize counter
}
void tictoc::tic()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    m_counterStart = li.QuadPart;
}
void tictoc::toc(LPCTSTR lpszFormat, ...)
{
    LARGE_INTEGER li, frq;
    QueryPerformanceCounter(&li);
    double PCFreq = 1.0;
    if (!QueryPerformanceFrequency(&frq))
    {
        OutputDebugString(L"%s: QueryPerformanceFrequency failed !\n");
        return;
    }
    else
    {
        PCFreq = double(frq.QuadPart) / 1000.0; // adjust denominator to change unit [1000.0 -> mSec]
    }
#if 0
    va_list args;
    va_start(args, lpszFormat);
    int nBuf;

    TCHAR szBuffer[MAX_LENGTH]; // get rid of this hard-coded buffer
    StringCbVPrintf(szBuffer, MAX_LENGTH - 1, L"{%d}[time-taken:\t%lf mSec] ", GetCurrentThreadId(), double(li.QuadPart - m_counterStart) / PCFreq);
    nBuf = _tcslen(szBuffer);

    nBuf = _vsntprintf_s(szBuffer, 511, lpszFormat, args);
    nBuf = _vsntprintf(&szBuffer[nBuf], 720 - nBuf, lpszFormat, args);
    ::OutputDebugString(szBuffer);
    va_end(args);
#endif

    tic(); // enables cascading time measurements
}
void tictoc::elapsed(LPCTSTR lpszFormat, ...)
{
    LARGE_INTEGER li, frq;
    QueryPerformanceCounter(&li);
    double PCFreq = 1.0;
    if (!QueryPerformanceFrequency(&frq))
    {
        DBGMSG(dbgINFO, L"QueryPerformanceFrequency failed!");
        return;
    }
    else
    {
        PCFreq = double(frq.QuadPart) / 1000.0; // adjust denominator to change unit
    }

    va_list args;
    va_start(args, lpszFormat);
    std::size_t nBuf;
    std::size_t bufferSize;

    char tempBuffer[MAX_LENGTH];
    TCHAR szBuffer[MAX_LENGTH];

    sprintf_s(tempBuffer, MAX_LENGTH-1, "{%x - Thread} [time-taken:%lf mecs]", GetCurrentThreadId(), double(li.QuadPart - m_counterStart) / PCFreq);
    nBuf = strlen(tempBuffer);

    mbstowcs_s(&bufferSize, szBuffer, MAX_LENGTH-1, tempBuffer, MAX_LENGTH-1);
    nBuf = _tcslen(szBuffer);

    nBuf = _vsntprintf_s(&szBuffer[nBuf], MAX_LENGTH-nBuf, MAX_LENGTH-1, lpszFormat, args);

    DBGMSG(dbgINFO, L"%s - %s", WFUNCTION, szBuffer);
    va_end(args);
}

double tictoc::getelapsed()
{
    LARGE_INTEGER li, frq;
    QueryPerformanceCounter(&li);
    double PCFreq = 1.0;
    if (!QueryPerformanceFrequency(&frq))
    {
        OutputDebugString(L"QueryPerformanceFrequency failed !\n");
        return 0.00f;
    }
    else
    {
        PCFreq = double(frq.QuadPart) / 1000.0; // adjust denominator to change unit
    }

    return double(li.QuadPart - m_counterStart) / PCFreq;
}

//--------------------------------------------------------------------------------------
// Name: IsWin7OrGreater
// Description: checks for a win7or greater system
//--------------------------------------------------------------------------------------
bool IsWin7OrGreater(void)
{
    bool bIsWin7OrGreater = false;
    OSVERSIONINFO OsVer;

    memset(&OsVer, 0, sizeof(OSVERSIONINFO));
    OsVer.dwOSVersionInfoSize  = sizeof(OSVERSIONINFO);
    GetVersionEx(&OsVer);

    // win10 is having major version as 10 and minor version as 0
    if (OsVer.dwMajorVersion >= 10)
    {
        bIsWin7OrGreater = true;
    }
    else if ((OsVer.dwMajorVersion >= 6) && (OsVer.dwMinorVersion >= 1))
    {
        bIsWin7OrGreater = true;
    }
    DBGMSG(dbgERROR, L"%s: OS Info(%d.%d) ret = %d", WFUNCTION, OsVer.dwMajorVersion, OsVer.dwMinorVersion, bIsWin7OrGreater);
    return bIsWin7OrGreater;
} //IsWin7OrGreater

//--------------------------------------------------------------------------------------
// Name: IsWin7
// Description: checks for a win7system
//--------------------------------------------------------------------------------------
bool IsWin7(void)
{
    bool bIsWin7 = false;
    OSVERSIONINFO OsVer;

    memset(&OsVer, 0, sizeof(OSVERSIONINFO));
    OsVer.dwOSVersionInfoSize  = sizeof(OSVERSIONINFO);
    GetVersionEx(&OsVer);

    if ((OsVer.dwMajorVersion == 6) && (OsVer.dwMinorVersion == 1))
    {
        bIsWin7 = true;
    }
    DBGMSG(dbgERROR, L"%s: OS Info(%d.%d) ret = %d", WFUNCTION, OsVer.dwMajorVersion, OsVer.dwMinorVersion, bIsWin7);
    return bIsWin7;
} //IsWin7