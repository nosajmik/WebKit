/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "WebGPUAdapterImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDeviceImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace PAL::WebGPU {

static String adapterName(WGPUAdapter adapter)
{
    WGPUAdapterProperties properties;
    wgpuAdapterGetProperties(adapter, &properties);
    return properties.name;
}

static Ref<SupportedFeatures> supportedFeatures(WGPUAdapter adapter)
{
    auto featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    Vector<WGPUFeatureName> features(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, features.data());

    Vector<String> result;
    for (auto feature : features) {
        switch (feature) {
        case WGPUFeatureName_Undefined:
            continue;
        case WGPUFeatureName_DepthClipControl:
            result.append("depth-clip-control"_s);
            break;
        case WGPUFeatureName_Depth24UnormStencil8:
            result.append("depth24unorm-stencil8"_s);
            break;
        case WGPUFeatureName_Depth32FloatStencil8:
            result.append("depth32float-stencil8"_s);
            break;
        case WGPUFeatureName_TimestampQuery:
            result.append("timestamp-query"_s);
            break;
        case WGPUFeatureName_PipelineStatisticsQuery:
            result.append("pipeline-statistics-query"_s);
            break;
        case WGPUFeatureName_TextureCompressionBC:
            result.append("texture-compression-bc"_s);
            break;
        case WGPUFeatureName_TextureCompressionETC2:
            result.append("texture-compression-etc2"_s);
            break;
        case WGPUFeatureName_TextureCompressionASTC:
            result.append("texture-compression-astc"_s);
            break;
        case WGPUFeatureName_IndirectFirstInstance:
            result.append("indirect-first-instance"_s);
            break;
        default:
            continue;
        }
    }
    return SupportedFeatures::create(WTFMove(result));
}

static Ref<SupportedLimits> supportedLimits(WGPUAdapter adapter)
{
    WGPUSupportedLimits limits;
    limits.nextInChain = nullptr;
    auto result = wgpuAdapterGetLimits(adapter, &limits);
    ASSERT_UNUSED(result, result);
    return SupportedLimits::create(
        limits.limits.maxTextureDimension1D,
        limits.limits.maxTextureDimension2D,
        limits.limits.maxTextureDimension3D,
        limits.limits.maxTextureArrayLayers,
        limits.limits.maxBindGroups,
        limits.limits.maxDynamicUniformBuffersPerPipelineLayout,
        limits.limits.maxDynamicStorageBuffersPerPipelineLayout,
        limits.limits.maxSampledTexturesPerShaderStage,
        limits.limits.maxSamplersPerShaderStage,
        limits.limits.maxStorageBuffersPerShaderStage,
        limits.limits.maxStorageTexturesPerShaderStage,
        limits.limits.maxUniformBuffersPerShaderStage,
        limits.limits.maxUniformBufferBindingSize,
        limits.limits.maxStorageBufferBindingSize,
        limits.limits.minUniformBufferOffsetAlignment,
        limits.limits.minStorageBufferOffsetAlignment,
        limits.limits.maxVertexBuffers,
        limits.limits.maxVertexAttributes,
        limits.limits.maxVertexBufferArrayStride,
        limits.limits.maxInterStageShaderComponents,
        limits.limits.maxComputeWorkgroupStorageSize,
        limits.limits.maxComputeInvocationsPerWorkgroup,
        limits.limits.maxComputeWorkgroupSizeX,
        limits.limits.maxComputeWorkgroupSizeY,
        limits.limits.maxComputeWorkgroupSizeZ,
        limits.limits.maxComputeWorkgroupsPerDimension);
}

static bool isFallbackAdapter(WGPUAdapter adapter)
{
    WGPUAdapterProperties properties;
    wgpuAdapterGetProperties(adapter, &properties);
    return properties.adapterType == WGPUAdapterType_CPU;
}

AdapterImpl::AdapterImpl(WGPUAdapter adapter, ConvertToBackingContext& convertToBackingContext)
    : Adapter(adapterName(adapter), supportedFeatures(adapter), supportedLimits(adapter), WebGPU::isFallbackAdapter(adapter))
    , m_backing(adapter)
    , m_convertToBackingContext(convertToBackingContext)
{
}

AdapterImpl::~AdapterImpl()
{
    wgpuAdapterRelease(m_backing);
}

void AdapterImpl::requestDevice(const DeviceDescriptor& descriptor, CompletionHandler<void(Ref<Device>&&)>&& callback)
{
    auto label = descriptor.label.utf8();

    auto features = descriptor.requiredFeatures.map([this] (auto featureName) {
        return m_convertToBackingContext->convertToBacking(featureName);
    });

    WGPURequiredLimits limits {
        nullptr, {
            0, // maxTextureDimension1D
            0, // maxTextureDimension2D
            0, // maxTextureDimension3D
            0, // maxTextureArrayLayers
            0, // maxBindGroups
            0, // maxDynamicUniformBuffersPerPipelineLayout
            0, // maxDynamicStorageBuffersPerPipelineLayout
            0, // maxSampledTexturesPerShaderStage
            0, // maxSamplersPerShaderStage
            0, // maxStorageBuffersPerShaderStage
            0, // maxStorageTexturesPerShaderStage
            0, // maxUniformBuffersPerShaderStage
            0, // maxUniformBufferBindingSize
            0, // maxStorageBufferBindingSize
            0, // minUniformBufferOffsetAlignment
            0, // minStorageBufferOffsetAlignment
            0, // maxVertexBuffers
            0, // maxVertexAttributes
            0, // maxVertexBufferArrayStride
            0, // maxInterStageShaderComponents
            0, // maxComputeWorkgroupStorageSize
            0, // maxComputeInvocationsPerWorkgroup
            0, // maxComputeWorkgroupSizeX
            0, // maxComputeWorkgroupSizeY
            0, // maxComputeWorkgroupSizeZ
            0, // maxComputeWorkgroupsPerDimension
        },
    };

    WGPUDeviceDescriptor backingDescriptor {
        nullptr,
        label.data(),
        static_cast<uint32_t>(features.size()),
        features.data(),
        &limits,
    };

    wgpuAdapterRequestDeviceWithBlock(m_backing, &backingDescriptor, makeBlockPtr([protectedThis = Ref { *this }, convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback)](WGPURequestDeviceStatus, WGPUDevice device, const char*) mutable {
        callback(DeviceImpl::create(device, Ref { protectedThis->features() }, Ref { protectedThis->limits() }, convertToBackingContext));
    }).get());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
