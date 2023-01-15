/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FETileSoftwareApplier.h"

#include "AffineTransform.h"
#include "FETile.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"

// For kpc_get_thread_counters: nosajmik
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>


namespace WebCore {

bool FETileSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
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

    auto& input = inputs[0].get();

    auto resultImage = result.imageBuffer();
    auto inputImage = input.imageBuffer();
    if (!resultImage || !inputImage)
        return false;

    auto inputImageRect = input.absoluteImageRect();
    auto resultImageRect = result.absoluteImageRect();

    auto tileRect = input.maxEffectRect(filter);
    tileRect.scale(filter.filterScale());

    auto maxResultRect = result.maxEffectRect(filter);
    maxResultRect.scale(filter.filterScale());

    auto tileImage = ImageBuffer::create(tileRect.size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, bufferOptionsForRendingMode(filter.renderingMode()));
    if (!tileImage)
        return false;

    auto& tileImageContext = tileImage->context();
    tileImageContext.translate(-tileRect.location());
    tileImageContext.drawImageBuffer(*inputImage, inputImageRect.location());

    AffineTransform patternTransform;
    patternTransform.translate(tileRect.location() - maxResultRect.location());

    auto pattern = Pattern::create({ tileImage.releaseNonNull() }, { true, true, patternTransform });

    auto& resultContext = resultImage->context();
    resultContext.setFillPattern(WTFMove(pattern));
    resultContext.fillRect(FloatRect(FloatPoint(), resultImageRect.size()));

    // Timestamp 2
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
    asm volatile ("isb sy");
    
    uint64_t dt = counters_after[2] - counters_before[2];
    fprintf(stderr, "FETileSoftwareApplier::apply took %llu cycles\n", dt);

    return true;
}

} // namespace WebCore
