// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_
#define ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_

#include <memory>

#include "v8/include/v8.h"

namespace base {
class ProcessMetrics;
}

namespace electron {

// Watches for worker contexts and insert node integration to it.
class SandboxedWorkerObserver {
 public:
  // Returns the SandboxedWorkerObserver for current worker thread.
  static SandboxedWorkerObserver* GetCurrent();

  // disable copy
  SandboxedWorkerObserver(const SandboxedWorkerObserver&) = delete;
  SandboxedWorkerObserver& operator=(const SandboxedWorkerObserver&) = delete;

  void DidInitializeWorkerContextOnWorkerThread(v8::Local<v8::Context> context);
  void ContextWillDestroy(v8::Local<v8::Context> context);

 private:
  SandboxedWorkerObserver();
  ~SandboxedWorkerObserver();

  std::unique_ptr<base::ProcessMetrics> metrics_;
};

}  // namespace electron

#endif  // ELECTRON_SHELL_RENDERER_SANDBOXED_WORKER_OBSERVER_H_
