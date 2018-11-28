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

#ifndef _NVENCBROADCAST_OSDEFINES_H
#define _NVENCBROADCAST_OSDEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32)
#ifndef WIN_OS
#define WIN_OS 1
#endif
#elif defined(__APPLE__)
#ifndef OS_MACOSX
#define OS_MACOSX 1
#endif
#elif defined(__linux__)
#ifndef OS_LINUX
#define OS_LINUX 1
#endif
#else
#error Add the appropriate construct for the platform complier 
#endif

#if defined(WIN_OS)
#include <Windows.h>
#endif

#ifndef NVENCBROADCAST_NOVTABLE
#if defined(WIN_OS)
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define NVENCBROADCAST_NOVTABLE  __declspec(novtable)
#else
#define NVENCBROADCAST_NOVTABLE
#endif
#else
#error Add the appropriate construct for the platform complier 
#endif
#endif

#ifndef NVENCBROADCAST_EXPORT
#if defined(WIN32) 
#define NVENCBROADCAST_EXPORT  __declspec(dllexport)
#define NVENCBROADCAST_Func    __stdcall
#elif
#define NVENCBROADCAST_EXPORT
#endif
#endif

#ifdef __cplusplus
};
#endif

#endif //_NVENCBROADCAST_OSDEFINES_H

