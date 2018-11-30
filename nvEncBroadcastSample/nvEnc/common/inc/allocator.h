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

#include <memory>

template<class T>
class Allocator {
public:
    // function members
    static T* allocate(uint32_t n);
    static void deallocate(T* p, uint32_t n, bool bArray = false);
};

template <class T>
T* Allocator<T>::allocate(uint32_t n) {
    T* ptr = nullptr;
    try {
        ptr = new T[n * sizeof(T)];
    } catch (std::bad_alloc& e) {
        throw e;
    }
    return ptr;
}

template <class T>
void Allocator<T>::deallocate(T* mem_ptr, uint32_t n, bool bArray = false) {
    UNREFERENCED_PARAMETER(n);
    if (bArray)
        delete[] mem_ptr;
    else
        delete mem_ptr;
}