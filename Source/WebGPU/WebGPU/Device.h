/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#pragma once

#import "Queue.h"
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/RefPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

struct WGPUDeviceImpl {
};

namespace WebGPU {

class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class Instance;
class PipelineLayout;
class QuerySet;
class RenderBundleEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Surface;
class SwapChain;
class Texture;

class Device : public WGPUDeviceImpl, public RefCounted<Device> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<Device> create(id<MTLDevice>, String&& deviceLabel, Instance&);

    ~Device();

    RefPtr<BindGroup> createBindGroup(const WGPUBindGroupDescriptor&);
    RefPtr<BindGroupLayout> createBindGroupLayout(const WGPUBindGroupLayoutDescriptor&);
    RefPtr<Buffer> createBuffer(const WGPUBufferDescriptor&);
    RefPtr<CommandEncoder> createCommandEncoder(const WGPUCommandEncoderDescriptor&);
    RefPtr<ComputePipeline> createComputePipeline(const WGPUComputePipelineDescriptor&);
    void createComputePipelineAsync(const WGPUComputePipelineDescriptor&, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, RefPtr<ComputePipeline>&&, String&& message)>&& callback);
    RefPtr<PipelineLayout> createPipelineLayout(const WGPUPipelineLayoutDescriptor&);
    RefPtr<QuerySet> createQuerySet(const WGPUQuerySetDescriptor&);
    RefPtr<RenderBundleEncoder> createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor&);
    RefPtr<RenderPipeline> createRenderPipeline(const WGPURenderPipelineDescriptor&);
    void createRenderPipelineAsync(const WGPURenderPipelineDescriptor&, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, RefPtr<RenderPipeline>&&, String&& message)>&& callback);
    RefPtr<Sampler> createSampler(const WGPUSamplerDescriptor&);
    RefPtr<ShaderModule> createShaderModule(const WGPUShaderModuleDescriptor&);
    RefPtr<SwapChain> createSwapChain(const Surface&, const WGPUSwapChainDescriptor&);
    RefPtr<Texture> createTexture(const WGPUTextureDescriptor&);
    void destroy();
    size_t enumerateFeatures(WGPUFeatureName* features);
    bool getLimits(WGPUSupportedLimits&);
    Queue& getQueue();
    bool hasFeature(WGPUFeatureName);
    bool popErrorScope(CompletionHandler<void(WGPUErrorType, String&&)>&& callback);
    void pushErrorScope(WGPUErrorFilter);
    void setDeviceLostCallback(Function<void(WGPUDeviceLostReason, String&&)>&&);
    void setUncapturedErrorCallback(Function<void(WGPUErrorType, String&&)>&&);
    void setLabel(String&&);

    id<MTLDevice> device() const { return m_device; }

    void generateAValidationError(String&& message);

    Instance& instance() const { return m_instance; }
    bool hasUnifiedMemory() const { return m_device.hasUnifiedMemory; }

private:
    Device(id<MTLDevice>, id<MTLCommandQueue> defaultQueue, Instance&);

    struct ErrorScope;
    ErrorScope* currentErrorScope(WGPUErrorFilter);
    bool validatePopErrorScope() const;

    struct Error {
        WGPUErrorType type;
        String message;
    };
    struct ErrorScope {
        std::optional<Error> error; // "The first GPUError, if any, observed while the GPU error scope was current."
        const WGPUErrorFilter filter; // Determines what type of GPUError this GPU error scope observes.
    };

    const id<MTLDevice> m_device { nil };
    const Ref<Queue> m_defaultQueue;
    const Ref<Instance> m_instance;

    Function<void(WGPUErrorType, String&&)> m_uncapturedErrorCallback;
    Vector<ErrorScope> m_errorScopeStack; // "A stack of GPU error scopes that have been pushed to the GPUDevice."
};

} // namespace WebGPU
