/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "FEBlendSoftwareApplier.h"

#include "FEBlend.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

// For kpc_get_thread_counters: nosajmik
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>

namespace WebCore {

bool FEBlendSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
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

    auto resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto inputImage = input.imageBuffer();
    auto inputImage2 = input2.imageBuffer();
    if (!inputImage || !inputImage2)
        return false;

    auto& filterContext = resultImage->context();
    auto inputImageRect = input.absoluteImageRectRelativeTo(result);
    auto inputImageRect2 = input2.absoluteImageRectRelativeTo(result);

    filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
    filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, { CompositeOperator::SourceOver, m_effect.blendMode() });

    // Timestamp 2
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
    asm volatile ("isb sy");
    
    uint64_t dt = counters_after[2] - counters_before[2];
    fprintf(stderr, "FEBlendSoftwareApplier::apply took %llu cycles\n", dt);

    return true;
}

} // namespace WebCore
