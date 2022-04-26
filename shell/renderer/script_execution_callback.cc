// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/renderer/script_execution_callback.h"

#include "shell/common/gin_converters/blink_converter.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/function_template_extensions.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/renderer/api/context_bridge/object_cache.h"
#include "shell/renderer/api/electron_api_context_bridge.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "v8/include/v8.h"

namespace electron {

ScriptExecutionCallback::ScriptExecutionCallback(
    CompleteCallback callback,
    v8::Local<v8::Context> script_context)
    : callback_(std::move(callback)), script_context_(script_context) {}

ScriptExecutionCallback::~ScriptExecutionCallback() {
}

void ScriptExecutionCallback::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  v8::Isolate* isolate = script_context_->GetIsolate();
  if (!result.empty()) {
    if (!result[0].IsEmpty()) {
      v8::Local<v8::Value> value = result[0];
      // Either the result was created in the same world as the caller
      // or the result is not an object and therefore does not have a
      // prototype chain to protect
      bool should_clone_value =
          !(value->IsObject() &&
            script_context_ ==
                value.As<v8::Object>()->CreationContext()) &&
          value->IsObject();
      if (should_clone_value) {
        CopyResultToCallingContextAndFinalize(isolate,
                                              value.As<v8::Object>());
      } else {
        // Right now only single results per frame is supported.
        if (callback_)
          std::move(callback_).Run(v8::Undefined(isolate), value);
      }
    } else {
      const char error_message[] =
          "Script failed to execute, this normally means an error "
          "was thrown. Check the renderer console for the error.";
      if (!callback_.is_null()) {
        v8::Context::Scope context_scope(script_context_);
        std::move(callback_).Run(
            v8::Exception::Error(
                v8::String::NewFromUtf8(isolate, error_message)
                    .ToLocalChecked()),
            v8::Undefined(isolate));
      }
    }
  } else {
    const char error_message[] =
        "WebFrame was removed before script could run. This normally means "
        "the underlying frame was destroyed";
    if (!callback_.is_null()) {
      v8::Context::Scope context_scope(script_context_);
      std::move(callback_).Run(
          v8::Undefined(isolate),
          v8::Exception::Error(v8::String::NewFromUtf8(isolate, error_message)
                                    .ToLocalChecked()));
    }
  }
  delete this;
}


void ScriptExecutionCallback::CopyResultToCallingContextAndFinalize(
    v8::Isolate* isolate,
    const v8::Local<v8::Object>& result) {
  v8::MaybeLocal<v8::Value> maybe_result;
  bool success = true;
  std::string error_message =
      "An unknown exception occurred while getting the result of the script";
  {
    v8::TryCatch try_catch(isolate);
    api::context_bridge::ObjectCache object_cache;
    maybe_result = api::PassValueToOtherContext(result->CreationContext(),
                                            script_context_, result,
                                            &object_cache, false, 0);
    if (maybe_result.IsEmpty() || try_catch.HasCaught()) {
      success = false;
    }
    if (try_catch.HasCaught()) {
      auto message = try_catch.Message();

      if (!message.IsEmpty()) {
        gin::ConvertFromV8(isolate, message->Get(), &error_message);
      }
    }
  }
  if (!success) {
    // Failed convert so we send undefined everywhere
    std::move(callback_).Run(
        v8::Exception::Error(
            v8::String::NewFromUtf8(isolate, error_message.c_str())
                .ToLocalChecked()),
        v8::Undefined(isolate));
  } else {
    v8::Context::Scope context_scope(script_context_);
    v8::Local<v8::Value> cloned_value = maybe_result.ToLocalChecked();
    std::move(callback_).Run(v8::Undefined(isolate), cloned_value);
  }
}

}  // namespace electron
