// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_RENDERER_SCRIPT_EXECUTION_CALLBACK_H_
#define ELECTRON_SHELL_RENDERER_SCRIPT_EXECUTION_CALLBACK_H_

#include <vector>

#include "base/callback.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-forward.h"

namespace electron {

// Script execution callback which will correctly sanatize the result to the
// appropriate calling context.
// This class manages its own lifetime.
class ScriptExecutionCallback : public blink::WebScriptExecutionCallback {
 public:
  using CompleteCallback =
      base::OnceCallback<void(const v8::Local<v8::Value>& error,
                              const v8::Local<v8::Value>& result)>;

  explicit ScriptExecutionCallback(
      CompleteCallback callback,
      v8::Local<v8::Context> script_context);

  ScriptExecutionCallback(const ScriptExecutionCallback&) = delete;
  ScriptExecutionCallback& operator=(const ScriptExecutionCallback&) = delete;

  ~ScriptExecutionCallback() override;

  void Completed(const blink::WebVector<v8::Local<v8::Value>>& result) override;

 private:
  void CopyResultToCallingContextAndFinalize(
      v8::Isolate* isolate,
      const v8::Local<v8::Object>& result);

  CompleteCallback callback_;
  // TODO: ensure script context lifespan
  v8::Local<v8::Context> script_context_;
};

}  // namespace extensions

#endif  // ELECTRON_SHELL_RENDERER_SCRIPT_EXECUTION_CALLBACK_H_
