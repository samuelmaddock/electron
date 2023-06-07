// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_
#define ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"  // nogncheck
#include "v8/include/v8.h"

namespace base {
class ProcessMetrics;
}

namespace blink {
class DOMWrapperWorld;
class ScriptState;
class WorkerOrWorkletGlobalScope;
}  // namespace blink

namespace electron {

// Watches for worker contexts and insert node integration to it.
class SandboxedWorkerObserver {
 public:
  // Returns the SandboxedWorkerObserver for current worker thread.
  static SandboxedWorkerObserver* GetCurrent();

  // disable copy
  SandboxedWorkerObserver(const SandboxedWorkerObserver&) = delete;
  SandboxedWorkerObserver& operator=(const SandboxedWorkerObserver&) = delete;

  void InitializePreloadContextOnWorkerThread(
      v8::Isolate* isolate,
      v8::Local<v8::Context> worker_context);

  void DidInitializeWorkerContextOnWorkerThread(v8::Local<v8::Context> context);
  void ContextWillDestroy(v8::Local<v8::Context> context);

 private:
  SandboxedWorkerObserver();
  ~SandboxedWorkerObserver();

  std::unique_ptr<base::ProcessMetrics> metrics_;

  // blink::WorkerOrWorkletGlobalScope* global_scope_;
  blink::Persistent<blink::ScriptState> script_state_;
  v8::Global<v8::Context> preload_context_;
  scoped_refptr<blink::DOMWrapperWorld> world_;
};

}  // namespace electron

#endif  // ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_
