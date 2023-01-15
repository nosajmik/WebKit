/*
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FEImageSoftwareApplier.h"

#include "FEImage.h"
#include "Filter.h"
#include "GraphicsContext.h"

// For kpc_get_thread_counters: nosajmik
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>

namespace WebCore {

bool FEImageSoftwareApplier::apply(const Filter& filter, const FilterImageVector&, FilterImage& result) const
{
    // jsc or WebKit MUST BE RUN AS ROOT for this to work.
    const char *kperf_path = "/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf";
    void *kperf_lib = NULL;
    int (*kpc_get_thread_counters)(int, unsigned, uint64_t *) = NULL;

    // The array size is the size of the entire array divided by the size of the
    // first element, i.e. this macro expands to the number of elements in the
    // array.
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

    // We cannot open the KPC API provided by the kernel ourselves directly.
    // Instead we rely on the kperf framework which is entitled to access
    // this API.
    kperf_lib = dlopen(kperf_path, RTLD_LAZY);
    
    if (!kperf_lib) {
        fprintf(stderr, "Couldn't open /System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf. Is WebKit running with root privileges?\n");
    }

    // Look up kpc_get_thread_counters.
    // Need to do some casting here because compiler will complain about
    // assigning void pointer to function pointer
    *(void **)(&kpc_get_thread_counters) = dlsym(kperf_lib, "kpc_get_thread_counters");

    // Storage space for performance counters on two timestamps.
    // Read with serialization on both sides.
    uint64_t counters_before[10];
    uint64_t counters_after[10];
    
    // Timestamp 1
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_before), counters_before);
    asm volatile ("isb sy");

    bool ret;

    auto resultImage = result.imageBuffer();
    if (!resultImage)
        ret = false;

    auto& sourceImage = m_effect.sourceImage();
    auto primitiveSubregion = result.primitiveSubregion();
    auto& context = resultImage->context();

    if (auto nativeImage = sourceImage.nativeImageIfExists()) {
        auto imageRect = primitiveSubregion;
        auto srcRect = m_effect.sourceImageRect();
        m_effect.preserveAspectRatio().transformRect(imageRect, srcRect);
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawNativeImage(*nativeImage, srcRect.size(), imageRect, srcRect);
        ret = true;
    }

    if (auto imageBuffer = sourceImage.imageBufferIfExists()) {
        auto imageRect = primitiveSubregion;
        imageRect.moveBy(m_effect.sourceImageRect().location());
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawImageBuffer(*imageBuffer, imageRect.location());
        ret = true;
    }

    // Timestamp 2
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
    asm volatile ("isb sy");
    
    uint64_t dt = counters_after[2] - counters_before[2];
    fprintf(stderr, "FEImageSoftwareApplier::apply took %llu cycles\n", dt);

    return ret;
    
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
