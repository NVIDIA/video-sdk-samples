################################################################################
#
# Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.
#
################################################################################

# Common definitions
GCC ?= g++

CCFLAGS := -std=c++11

# Debug build flags
ifeq ($(dbg),1)
    CCFLAGS += -g
endif

CUDA_PATH ?= /usr/local/cuda

# Link applications against stub libraries provided in the SDKs.
LDFLAGS := -L$(CUDA_PATH)/lib64/stubs
LDFLAGS += -L../../NvCodec/Lib/linux/stubs/x86_64
LDFLAGS += -ldl -lcuda

NVCC ?= $(CUDA_PATH)/bin/nvcc

# Common includes and paths
INCLUDES := -I$(CUDA_PATH)/include
INCLUDES += -I../../NvCodec
INCLUDES += -I../../NvCodec/NvDecoder
INCLUDES += -I../../NvCodec/NvEncoder
INCLUDES += -I../../NvCodec/Common
