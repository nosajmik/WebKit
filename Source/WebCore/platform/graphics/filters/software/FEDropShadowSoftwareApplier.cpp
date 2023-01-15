/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "FEDropShadowSoftwareApplier.h"

#include "FEDropShadow.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include "ShadowBlur.h"

// For kpc_get_thread_counters: nosajmik
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>

namespace WebCore {

bool FEDropShadowSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
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
    if (!resultImage)
        return false;

    auto stdDeviation = filter.resolvedSize({ m_effect.stdDeviationX(), m_effect.stdDeviationY() });
    auto blurRadius = 2 * filter.scaledByFilterScale(stdDeviation);
    
    auto offset = filter.resolvedSize({ m_effect.dx(), m_effect.dy() });
    auto absoluteOffset = filter.scaledByFilterScale(offset);

    FloatRect inputImageRect = input.absoluteImageRectRelativeTo(result);
    FloatRect inputImageRectWithOffset(inputImageRect);
    inputImageRectWithOffset.move(absoluteOffset);

    auto inputImage = input.imageBuffer();
    if (!inputImage)
        return false;

    auto& resultContext = resultImage->context();
    resultContext.setAlpha(m_effect.shadowOpacity());
    resultContext.drawImageBuffer(*inputImage, inputImageRectWithOffset);
    resultContext.setAlpha(1);

    ShadowBlur contextShadow(blurRadius, absoluteOffset, m_effect.shadowColor());

    PixelBufferFormat format { AlphaPremultiplication::Premultiplied, PixelFormat::RGBA8, result.colorSpace() };
    IntRect shadowArea(IntPoint(), resultImage->truncatedLogicalSize());
    auto pixelBuffer = resultImage->getPixelBuffer(format, shadowArea);
    if (!pixelBuffer)
        return false;

    contextShadow.blurLayerImage(pixelBuffer->bytes(), pixelBuffer->size(), 4 * pixelBuffer->size().width());

    resultImage->putPixelBuffer(*pixelBuffer, shadowArea);

    resultContext.setCompositeOperation(CompositeOperator::SourceIn);
    resultContext.fillRect(FloatRect(FloatPoint(), result.absoluteImageRect().size()), m_effect.shadowColor());
    resultContext.setCompositeOperation(CompositeOperator::DestinationOver);

    resultImage->context().drawImageBuffer(*inputImage, inputImageRect);

    // Timestamp 2
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
    asm volatile ("isb sy");
    
    uint64_t dt = counters_after[2] - counters_before[2];
    fprintf(stderr, "FEDropShadowSoftwareApplier::apply took %llu cycles\n", dt);

    return true;
}

} // namespace WebCore
