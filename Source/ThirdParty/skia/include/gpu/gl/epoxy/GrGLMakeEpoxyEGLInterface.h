/*
 * Copyright 2024 Igalia S.L.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/gpu/GrTypes.h"

#include "include/core/SkRefCnt.h"

struct GrGLInterface;

sk_sp<const GrGLInterface> GrGLMakeEpoxyEGLInterface();
