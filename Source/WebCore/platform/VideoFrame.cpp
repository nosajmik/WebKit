/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VideoFrame.h"

#if PLATFORM(COCOA)
#include "VideoFrameCV.h"
#endif

#if ENABLE(VIDEO)

namespace WebCore {

VideoFrame::VideoFrame(MediaTime presentationTime, bool isMirrored, VideoRotation rotation)
    : m_presentationTime(presentationTime)
    , m_isMirrored(isMirrored)
    , m_rotation(rotation)
{
}

VideoFrame::~VideoFrame() = default;

MediaTime VideoFrame::presentationTime() const
{
    return m_presentationTime;
}

VideoFrame::Rotation VideoFrame::rotation() const
{
    return m_rotation;
}

bool VideoFrame::isMirrored() const
{
    return m_isMirrored;
}

WebCore::PlatformSample VideoFrame::platformSample() const
{
    return { WebCore::PlatformSample::VideoFrameType, { } };
}

PlatformSample::Type VideoFrame::platformSampleType() const
{
    return WebCore::PlatformSample::VideoFrameType;
}

MediaTime VideoFrame::decodeTime() const
{
    ASSERT_NOT_REACHED();
    return { };
}

MediaTime VideoFrame::duration() const
{
    ASSERT_NOT_REACHED();
    return { };
}

AtomString VideoFrame::trackID() const
{
    ASSERT_NOT_REACHED();
    return { };
}

size_t VideoFrame::sizeInBytes() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

void VideoFrame::offsetTimestampsBy(const MediaTime&)
{
    ASSERT_NOT_REACHED();
}

void VideoFrame::setTimestamps(const MediaTime&, const MediaTime&)
{
    ASSERT_NOT_REACHED();
}

Ref<WebCore::MediaSample> VideoFrame::createNonDisplayingCopy() const
{
    CRASH();
}

MediaSample::SampleFlags VideoFrame::flags() const
{
    return MediaSample::SampleFlags::None;
}

void VideoFrame::dump(PrintStream&) const
{
}

void VideoFrame::initializeCharacteristics(MediaTime presentationTime, bool isMirrored, VideoRotation rotation)
{
    const_cast<MediaTime&>(m_presentationTime) = presentationTime;
    const_cast<bool&>(m_isMirrored) = isMirrored;
    const_cast<VideoRotation&>(m_rotation) = rotation;
}

#if PLATFORM(COCOA)
RefPtr<VideoFrameCV> VideoFrame::asVideoFrameCV()
{
    auto buffer = pixelBuffer();
    if (!buffer)
        return nullptr;

    return VideoFrameCV::create(presentationTime(), isMirrored(), rotation(), buffer);
}
#endif

}

#endif
