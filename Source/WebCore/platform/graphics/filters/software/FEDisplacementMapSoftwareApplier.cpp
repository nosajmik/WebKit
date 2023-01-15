/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "FEDisplacementMapSoftwareApplier.h"

#include "FEDisplacementMap.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "PixelBuffer.h"

// For kpc_get_thread_counters: nosajmik
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>

namespace WebCore {

FEDisplacementMapSoftwareApplier::FEDisplacementMapSoftwareApplier(const FEDisplacementMap& effect)
    : Base(effect)
{
    ASSERT(m_effect.xChannelSelector() != CHANNEL_UNKNOWN);
    ASSERT(m_effect.yChannelSelector() != CHANNEL_UNKNOWN);
}

int FEDisplacementMapSoftwareApplier::xChannelIndex() const
{
    return m_effect.xChannelSelector() - 1;
}

int FEDisplacementMapSoftwareApplier::yChannelIndex() const
{
    return m_effect.yChannelSelector() - 1;
}

bool FEDisplacementMapSoftwareApplier::apply(const Filter& filter, const FilterImageVector& inputs, FilterImage& result) const
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
    auto& input2 = inputs[1].get();

    auto destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Premultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto effectADrawingRect = result.absoluteImageRectRelativeTo(input);
    auto inputPixelBuffer = input.getPixelBuffer(AlphaPremultiplication::Premultiplied, effectADrawingRect);

    auto effectBDrawingRect = result.absoluteImageRectRelativeTo(input2);
    // The calculations using the pixel values from ‘in2’ are performed using non-premultiplied color values.
    auto displacementPixelBuffer = input2.getPixelBuffer(AlphaPremultiplication::Unpremultiplied, effectBDrawingRect);
    
    if (!inputPixelBuffer || !displacementPixelBuffer)
        return false;

    ASSERT(inputPixelBuffer->sizeInBytes() == displacementPixelBuffer->sizeInBytes());

    auto paintSize = result.absoluteImageRect().size();
    auto scale = filter.resolvedSize({ m_effect.scale(), m_effect.scale() });
    auto absoluteScale = filter.scaledByFilterScale(scale);

    float scaleForColorX = absoluteScale.width() / 255.0;
    float scaleForColorY = absoluteScale.height() / 255.0;
    float scaledOffsetX = 0.5 - absoluteScale.width() * 0.5;
    float scaledOffsetY = 0.5 - absoluteScale.height() * 0.5;
    
    int displacementChannelX = xChannelIndex();
    int displacementChannelY = yChannelIndex();

    int rowBytes = paintSize.width() * 4;

    for (int y = 0; y < paintSize.height(); ++y) {
        int lineStartOffset = y * rowBytes;

        for (int x = 0; x < paintSize.width(); ++x) {
            int destinationIndex = lineStartOffset + x * 4;
            
            int srcX = x + static_cast<int>(scaleForColorX * displacementPixelBuffer->item(destinationIndex + displacementChannelX) + scaledOffsetX);
            int srcY = y + static_cast<int>(scaleForColorY * displacementPixelBuffer->item(destinationIndex + displacementChannelY) + scaledOffsetY);

            unsigned* destinationPixelPtr = reinterpret_cast<unsigned*>(destinationPixelBuffer->bytes() + destinationIndex);
            if (srcX < 0 || srcX >= paintSize.width() || srcY < 0 || srcY >= paintSize.height()) {
                *destinationPixelPtr = 0;
                continue;
            }

            *destinationPixelPtr = *reinterpret_cast<unsigned*>(inputPixelBuffer->bytes() + byteOffsetOfPixel(srcX, srcY, rowBytes));
        }
    }

    // Timestamp 2
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
    asm volatile ("isb sy");
    
    uint64_t dt = counters_after[2] - counters_before[2];
    fprintf(stderr, "FEDisplacementMapSoftwareApplier::apply took %llu cycles\n", dt);

    return true;
}

} // namespace WebCore
