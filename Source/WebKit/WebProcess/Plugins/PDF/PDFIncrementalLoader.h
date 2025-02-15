/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)

#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Threading.h>

OBJC_CLASS PDFDocument;

namespace WebCore {
class NetscapePlugInStreamLoader;
}

namespace WebKit {

class ByteRangeRequest;
class PDFPluginBase;
class PDFPluginStreamLoaderClient;

using ByteRangeRequestIdentifier = uint64_t;
using DataRequestCompletionHandler = Function<void(const uint8_t*, size_t count)>;

class PDFIncrementalLoader : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFIncrementalLoader> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFIncrementalLoader);
    friend class ByteRangeRequest;
    friend class PDFPluginStreamLoaderClient;
public:
    ~PDFIncrementalLoader();

    static Ref<PDFIncrementalLoader> create(PDFPluginBase&);

    void clear();

    void incrementalPDFStreamDidReceiveData(const WebCore::SharedBuffer&);
    void incrementalPDFStreamDidFinishLoading();
    void incrementalPDFStreamDidFail();

    void streamLoaderDidStart(ByteRangeRequestIdentifier, RefPtr<WebCore::NetscapePlugInStreamLoader>&&);

    void receivedNonLinearizedPDFSentinel();

#if !LOG_DISABLED
    void logState(WTF::TextStream&);
#endif

    // Only public for the callbacks
    size_t dataProviderGetBytesAtPosition(void* buffer, off_t position, size_t count);
    void dataProviderGetByteRanges(CFMutableArrayRef buffers, const CFRange* ranges, size_t count);

private:
    PDFIncrementalLoader(PDFPluginBase&);

    void threadEntry(Ref<PDFIncrementalLoader>&&);
    void transitionToMainThreadDocument();

    bool documentFinishedLoading() const;

    void ensureDataBufferLength(uint64_t);
    void appendAccumulatedDataToDataBuffer(ByteRangeRequest&);

    const uint8_t* dataPtrForRange(uint64_t position, size_t count) const;
    uint64_t availableDataSize() const;

    void getResourceBytesAtPosition(size_t count, off_t position, DataRequestCompletionHandler&&);
    size_t getResourceBytesAtPositionAfterLoadingComplete(void* buffer, off_t position, size_t count);

    void unconditionalCompleteOutstandingRangeRequests();

    ByteRangeRequest* byteRangeRequestForStreamLoader(WebCore::NetscapePlugInStreamLoader&);
    void forgetStreamLoader(WebCore::NetscapePlugInStreamLoader&);
    void cancelAndForgetStreamLoader(WebCore::NetscapePlugInStreamLoader&);

    ByteRangeRequestIdentifier identifierForLoader(WebCore::NetscapePlugInStreamLoader*);
    void removeOutstandingByteRangeRequest(ByteRangeRequestIdentifier);


    bool requestCompleteIfPossible(ByteRangeRequest&);
    void requestDidCompleteWithBytes(ByteRangeRequest&, size_t byteCount);
    void requestDidCompleteWithAccumulatedData(ByteRangeRequest&, size_t completionSize);

#if !LOG_DISABLED
    size_t incrementThreadsWaitingOnCallback() { return ++m_threadsWaitingOnCallback; }
    size_t decrementThreadsWaitingOnCallback() { return --m_threadsWaitingOnCallback; }

    void incrementalLoaderLog(const String&);
    void logStreamLoader(WTF::TextStream&, WebCore::NetscapePlugInStreamLoader&);
#endif

    ThreadSafeWeakPtr<PDFPluginBase> m_plugin;

    RetainPtr<PDFDocument> m_backgroundThreadDocument;
    RefPtr<Thread> m_pdfThread;

    Ref<PDFPluginStreamLoaderClient> m_streamLoaderClient;

    struct RequestData;
    std::unique_ptr<RequestData> m_requestData;

#if !LOG_DISABLED
    std::atomic<size_t> m_threadsWaitingOnCallback { 0 };
    std::atomic<size_t> m_completedRangeRequests { 0 };
    std::atomic<size_t> m_completedNetworkRangeRequests { 0 };
#endif


};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN) && HAVE(INCREMENTAL_PDF_APIS)
