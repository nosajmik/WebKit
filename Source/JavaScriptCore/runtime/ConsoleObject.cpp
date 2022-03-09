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

/*
 nosajmik: add declaration for describe() port from JSC console
 and cpuClflush() port from Dollar VM, "jlflush"
 */
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncDescribe);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncJlflush);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncTypeflush);
static JSC_DECLARE_HOST_FUNCTION(consoleProtoFuncRdtsc);

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

    /*
     nosajmik: add function handle for describe() port from JSC
     and jlflush from $vm
     */
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("describe", consoleProtoFuncDescribe, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("jlflush", consoleProtoFuncJlflush, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("typeflush", consoleProtoFuncTypeflush, static_cast<unsigned>(PropertyAttribute::None), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("rdtsc", consoleProtoFuncRdtsc, static_cast<unsigned>(PropertyAttribute::None), 0);

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

/*
 nosajmik: host function definition for describe() port from JSC console
 */
JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncDescribe, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    if (callFrame->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(vm, toString(callFrame->argument(0))));
}

/*
 nosajmik: host function definition for cpuClflush() port from Dollar VM
 */
// This takes either a JSArrayBuffer, JSArrayBufferView*, or any other object as its first
// argument. The second argument is expected to be an integer.
//
// If the first argument is a JSArrayBuffer, it'll clflush on that buffer
// plus the second argument as a byte offset. It'll also flush on the object
// itself so its length, etc, aren't in the cache.
//
// If the first argument is not a JSArrayBuffer, we load the butterfly
// and clflush at the address of the butterfly.
JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncJlflush, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    if (!callFrame->argument(1).isUInt32())
        return JSValue::encode(jsBoolean(false));

    auto clflush = [] (void* ptr) {
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    Vector<void*> toFlush;

    uint32_t offset = callFrame->argument(1).asUInt32();

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, callFrame->argument(0)))
        toFlush.append(bitwise_cast<char*>(view->vector()) + offset);
    else if (JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0))) {
        switch (object->indexingType()) {
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
            toFlush.append(bitwise_cast<char*>(object->butterfly()) + Butterfly::offsetOfVectorLength());
            toFlush.append(bitwise_cast<char*>(object->butterfly()) + Butterfly::offsetOfPublicLength());
        }
    } else if (JSString* string = jsDynamicCast<JSString*>(vm, callFrame->argument(0))) {
        // nosajmik: flush length of JSString.
        toFlush.append(bitwise_cast<char*>(string->getValueImpl()) + string->getValueImpl()->lengthMemoryOffset());
    }

    if (!toFlush.size())
        return JSValue::encode(jsBoolean(false));

    for (void* ptr : toFlush)
        clflush(ptr);
    return JSValue::encode(jsBoolean(true));
}

/*
 nosajmik: host function definition for cpuClflush() port from Dollar VM
 for flushing the structureID ONLY, for type confusion
 */
JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncTypeflush, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto clflush = [] (void* ptr) {
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    Vector<void*> toFlush;

    // nosajmik: callFrame->argument() calls CallFrame::getArgumentUnsafe(),
    // which returns JSValue (not pointer to JSValue)! JSValue can directly check
    // if it is a JSCell itself without needing jsDynamicCast (this will throw an error).
    if (callFrame->argument(0).isCell()) {
        toFlush.append(bitwise_cast<char*>(callFrame->argument(0).asCell()) + callFrame->argument(0).asCell()->structureIDOffset());
    } else if (JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0))) {
        // This is only here to make the compiler shut up about unused vars/params.
        asm volatile ("nop");
    }

    if (!toFlush.size())
        return JSValue::encode(jsBoolean(false));

    for (void* ptr : toFlush)
        clflush(ptr);
    return JSValue::encode(jsBoolean(true));
}

JSC_DEFINE_HOST_FUNCTION(consoleProtoFuncRdtsc, (JSGlobalObject*, CallFrame*))
{
#if CPU(X86_64) && !OS(WINDOWS)
    unsigned high;
    unsigned low;
    asm volatile ("rdtsc" : "=a"(low), "=d"(high));
    return JSValue::encode(jsNumber(low));
#else
    return JSValue::encode(jsNumber(0));
#endif
}

} // namespace JSC
