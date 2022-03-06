/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
#include "ConsoleObject.h"

#include "ConsoleClient.h"
#include "JSCInlines.h"
#include "ScriptArguments.h"
#include "ScriptCallStackFactory.h"

// nosajmik: includes for Stephan's high res M1 timer
#include <dlfcn.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

namespace JSC {

static String valueOrDefaultLabelString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    if (callFrame->argumentCount() < 1)
        return "default"_s;

    auto value = callFrame->argument(0);
    if (value.isUndefined())
        return "default"_s;

    return value.toWTFString(globalObject);
}

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ConsoleObject);

static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncDebug);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncError);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncLog);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncInfo);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncWarn);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncClear);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncDir);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncDirXML);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTable);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTrace);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncAssert);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncCount);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncCountReset);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncProfile);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncProfileEnd);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTakeHeapSnapshot);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTime);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTimeLog);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTimeEnd);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTimeStamp);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncGroup);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncGroupCollapsed);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncGroupEnd);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncRecord);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncRecordEnd);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncScreenshot);

// Stephan's high precision timer and instrument functions
// Porting to full Safari runtime
static JSC_DECLARE_HOST_FUNCTION(functionCpuRdtsc);
static JSC_DECLARE_HOST_FUNCTION(functionTimeWasmMemAccessM1);
static JSC_DECLARE_HOST_FUNCTION(functionGetuid);
static JSC_DECLARE_HOST_FUNCTION(functionGeteuid);

const ClassInfo ConsoleObject::s_info = { "console", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ConsoleObject) };

ConsoleObject::ConsoleObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void ConsoleObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    // For legacy reasons, console properties are enumerable, writable, deleteable,
    // and all have a length of 0. This may change if Console is standardized.

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("debug", consoleProtoFuncDebug, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("error", consoleProtoFuncError, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("log", consoleProtoFuncLog, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("info", consoleProtoFuncInfo, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("warn", consoleProtoFuncWarn, static_cast<unsigned>(PropertyAttribute::None), 0);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, consoleProtoFuncClear, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("dir", consoleProtoFuncDir, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("dirxml", consoleProtoFuncDirXML, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("table", consoleProtoFuncTable, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("trace", consoleProtoFuncTrace, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("assert", consoleProtoFuncAssert, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->count, consoleProtoFuncCount, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("countReset", consoleProtoFuncCountReset, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("profile", consoleProtoFuncProfile, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("profileEnd", consoleProtoFuncProfileEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("time", consoleProtoFuncTime, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeLog", consoleProtoFuncTimeLog, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeEnd", consoleProtoFuncTimeEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeStamp", consoleProtoFuncTimeStamp, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("takeHeapSnapshot", consoleProtoFuncTakeHeapSnapshot, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("group", consoleProtoFuncGroup, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("groupCollapsed", consoleProtoFuncGroupCollapsed, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("groupEnd", consoleProtoFuncGroupEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("record", consoleProtoFuncRecord, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("recordEnd", consoleProtoFuncRecordEnd, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("screenshot", consoleProtoFuncScreenshot, static_cast<unsigned>(PropertyAttribute::None), 0);

    // Stephan's high precision timer and instrument functions
    // These will become console.rdtsc() and console.timeAccess()
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("rdtsc", functionCpuRdtsc, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("timeAccess", functionTimeWasmMemAccessM1, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("getuid", functionGetuid, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("geteuid", functionGeteuid, static_cast<unsigned>(PropertyAttribute::None), 0);

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

static String valueToStringWithUndefinedOrNullCheck(JSGlobalObject* globalObject, JSValue value)
{
    if (value.isUndefinedOrNull())
        return String();
    return value.toWTFString(globalObject);
}

static EncodedJSValue consoleLogWithLevel(JSGlobalObject* globalObject, CallFrame* callFrame, MessageLevel level)
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->logWithLevel(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0), level);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncDebug, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return consoleLogWithLevel(globalObject, callFrame, MessageLevel::Debug);
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncError, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return consoleLogWithLevel(globalObject, callFrame, MessageLevel::Error);
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncLog, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return consoleLogWithLevel(globalObject, callFrame, MessageLevel::Log);
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncInfo, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return consoleLogWithLevel(globalObject, callFrame, MessageLevel::Info);
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncWarn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return consoleLogWithLevel(globalObject, callFrame, MessageLevel::Warning);
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncClear, (JSGlobalObject* globalObject, CallFrame*))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->clear(globalObject);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncDir, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->dir(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncDirXML, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->dirXML(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTable, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->table(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTrace, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->trace(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncAssert, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    bool condition = callFrame->argument(0).toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (condition)
        return JSValue::encode(jsUndefined());

    client->assertion(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 1));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncCount, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->count(globalObject, label);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncCountReset, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->countReset(globalObject, label);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncProfile, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->profile(globalObject, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(globalObject, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->profile(globalObject, title);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncProfileEnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->profileEnd(globalObject, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(globalObject, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->profileEnd(globalObject, title);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTakeHeapSnapshot, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    size_t argsCount = callFrame->argumentCount();
    if (!argsCount) {
        client->takeHeapSnapshot(globalObject, String());
        return JSValue::encode(jsUndefined());
    }

    const String& title(valueToStringWithUndefinedOrNullCheck(globalObject, callFrame->argument(0)));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->takeHeapSnapshot(globalObject, title);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->time(globalObject, label);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTimeLog, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->timeLog(globalObject, label, Inspector::createScriptArguments(globalObject, callFrame, 1));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTimeEnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    auto label = valueOrDefaultLabelString(globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    client->timeEnd(globalObject, label);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTimeStamp, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->timeStamp(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncGroup, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->group(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncGroupCollapsed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->groupCollapsed(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncGroupEnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->groupEnd(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncRecord, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->record(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncRecordEnd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->recordEnd(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncScreenshot, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto client = globalObject->consoleClient();
    if (!client)
        return JSValue::encode(jsUndefined());

    client->screenshot(globalObject, Inspector::createScriptArguments(globalObject, callFrame, 0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionCpuRdtsc, (JSGlobalObject*, CallFrame*))
{
    // jsc or WebKit MUST BE RUN AS ROOT for this to work.
    volatile const char *kperf_path = "/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf";
    volatile void *kperf_lib = NULL;
    volatile int (*kpc_get_thread_counters)(int, unsigned, uint64_t *) = NULL;

    // The array size is the size of the entire array divided by the size of the
    // first element, i.e. this macro expands to the number of elements in the
    // array.
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

    // We cannot open the KPC API provided by the kernel ourselves directly.
	// Instead we rely on the kperf framework which is entitled to access
	// this API.
	kperf_lib = dlopen(kperf_path, RTLD_LAZY);
    
    if (!kperf_lib) {
        // return undefined to user since printing doesn't work very well here
        return JSValue::encode(jsUndefined());
    }

    // Look up kpc_get_thread_counters.
    // Need to do some casting here because compiler will complain about
    // assigning void pointer to function pointer
	*(void **)(&kpc_get_thread_counters) = dlsym(kperf_lib, "kpc_get_thread_counters");

	// Read the counters BUT with serialization barrier
	volatile uint64_t counters[10];
    
    asm volatile ("isb sy");
    kpc_get_thread_counters(0, ARRAY_SIZE(counters), counters);
    asm volatile ("isb sy");

    return JSValue::encode(jsNumber(counters[2]));
}

/*
 This cheating function is similar to functionCpuRdtsc above
 (called from jsc as $vm.cpuRdtsc), but instead of simply returning
 the timestamp it will measure the access time to touch an index
 in wasm memory. Use as: $vm.timeWasmMemAccessM1(mem, wasmMemAddress)
 where mem is a WebAssembly.Memory object and wasmMemAddress is an int32.
 */
JSC_DEFINE_HOST_FUNCTION(functionTimeWasmMemAccessM1, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // jsc or WebKit MUST BE RUN AS ROOT for this to work.
    volatile const char *kperf_path = "/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf";
    volatile void *kperf_lib = NULL;
    volatile int (*kpc_get_thread_counters)(int, unsigned, uint64_t *) = NULL;

    // The array size is the size of the entire array divided by the size of the
    // first element, i.e. this macro expands to the number of elements in the
    // array.
    #define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

    // We cannot open the KPC API provided by the kernel ourselves directly.
	// Instead we rely on the kperf framework which is entitled to access
	// this API.
	kperf_lib = dlopen(kperf_path, RTLD_LAZY);
    
    if (!kperf_lib) {
        // return undefined to user since printing doesn't work very well here
        return JSValue::encode(jsUndefined());
    }

    // Look up kpc_get_thread_counters.
    // Need to do some casting here because compiler will complain about
    // assigning void pointer to function pointer
	*(void **)(&kpc_get_thread_counters) = dlsym(kperf_lib, "kpc_get_thread_counters");

    // Prep work to access variables passed in from JS runtime
    VM& vm = globalObject->vm();

	// Storage space for performance counters on two timestamps.
    // Read with serialization on both sides.
	volatile uint64_t counters_before[10];
    volatile uint64_t counters_after[10];

    // WebAssembly memory is an ArrayBuffer
    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, callFrame->argument(0))) {
        volatile void *vector = view->vector();
        // For testing in the future against PAC code, this may be worth trying
        // volatile void *vector = view->vectorWithoutPACValidation();

        volatile uint8_t *wasmMemoryBasePtr = static_cast<volatile uint8_t*>(vector);
        
        // Need to convert address from a NaN-boxed JSC value to int in C++
        JSValue addrValue = callFrame->argument(2);
        volatile uint32_t addr = addrValue.asUInt32();

        volatile uint8_t *target = wasmMemoryBasePtr + addr;

        // Timestamp 1
        asm volatile ("isb sy");
        kpc_get_thread_counters(0, ARRAY_SIZE(counters_before), counters_before);
        asm volatile ("isb sy");

        // Target access
        asm volatile("ldrb w0, [%[input]]" :: [input] "r" (target) : "w0");
        asm volatile("dsb ish"); // lfence

        // Timestamp 2
        asm volatile ("isb sy");
        kpc_get_thread_counters(0, ARRAY_SIZE(counters_after), counters_after);
        asm volatile ("isb sy");

        return JSValue::encode(jsNumber(counters_after[2] - counters_before[2]));
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionGetuid, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(getuid()));
}

JSC_DEFINE_HOST_FUNCTION(functionGeteuid, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(geteuid()));
}

} // namespace JSC
