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

DECODE_APPS := AppDec AppDecGL AppDecImageProvider AppDecLowLatency \
               AppDecMem AppDecMultiInput AppDecPerf AppDecMultiFiles

ENCODE_APPS := AppEncCuda AppEncDec AppEncGL AppEncLowLatency AppEncME \
               AppEncPerf AppEncQual

TRANSCODE_APPS := AppTrans AppTransOneToN AppTransPerf


APPS := $(addprefix AppDecode/,$(DECODE_APPS))
APPS += $(addprefix AppEncode/,$(ENCODE_APPS))
APPS += $(addprefix AppTranscode/,$(TRANSCODE_APPS))

.PHONY: build $(APPS)

build: $(APPS)

$(APPS):
	$(MAKE) -C $@

CLEAN := $(addsuffix .clean,$(APPS))

.PHONY: clean $(CLEAN)

clean: $(CLEAN)

$(CLEAN): %.clean:
	$(MAKE) -C $* clean
