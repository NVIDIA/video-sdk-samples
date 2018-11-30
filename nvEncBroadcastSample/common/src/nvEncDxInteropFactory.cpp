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

#include "nvEncAppUtils.h"
#include "nvEncDx9App.h"
#include "nvEncDx11App.h"
#include <memory>
//-------------------------------------------------------------------
// namespace CNvEncDxInteropFactory
//-------------------------------------------------------------------
namespace CNvEncDxInteropFactory
{
    //-------------------------------------------------------------------
    // CreateVideoEncoderInstance
    //-------------------------------------------------------------------
    std::unique_ptr<INvEncDxInterop> CreateDxInterop(DxContextType type)
    {
        std::unique_ptr<INvEncDxInterop> pDxNvEnc;
        pDxNvEnc.reset();
        switch (type)
        {
        case DxContextType::eDx11Type:
        {
            CNvEncDx11Interop*  pDx11Encode;
            pDx11Encode = nullptr;
            pDx11Encode = new CNvEncDx11Interop(); //todo have a createinstance function
            if (!pDx11Encode)
            {
                return nullptr;
            }
            pDxNvEnc.reset(pDx11Encode);
            break;
        }

        case DxContextType::eDx9Type:
        {
            CNvEncDx9Interop* pDx9Encode;
            pDx9Encode = nullptr;
            pDx9Encode = new CNvEncDx9Interop(); //todo have a createinstance function
            if (!pDx9Encode)
            {
                return nullptr;
            }
            pDxNvEnc.reset(pDx9Encode);
            break;
        }
        default:
            pDxNvEnc.reset();
            break;
        }
        return pDxNvEnc;
    } //CreateDxInterop
} //namespace