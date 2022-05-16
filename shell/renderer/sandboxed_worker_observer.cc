// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/renderer/sandboxed_worker_observer.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "shell/common/api/electron_bindings.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/event_emitter_caller.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "third_party/electron_node/src/node_binding.h"

namespace electron {

namespace {

const char kModuleCacheKey[] = "native-module-cache";

static base::LazyInstance<
    base::ThreadLocalPointer<SandboxedWorkerObserver>>::DestructorAtExit lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

v8::Local<v8::Object> GetModuleCache(v8::Isolate* isolate) {
  auto context = isolate->GetCurrentContext();
  gin_helper::Dictionary global(isolate, context->Global());
  v8::Local<v8::Value> cache;

  if (!global.GetHidden(kModuleCacheKey, &cache)) {
    cache = v8::Object::New(isolate);
    global.SetHidden(kModuleCacheKey, cache);
  }

  return cache->ToObject(context).ToLocalChecked();
}

// adapted from node.cc
v8::Local<v8::Value> GetBinding(v8::Isolate* isolate,
                                v8::Local<v8::String> key,
                                gin_helper::Arguments* margs) {
  v8::Local<v8::Object> exports;
  std::string module_key = gin::V8ToString(isolate, key);
  gin_helper::Dictionary cache(isolate, GetModuleCache(isolate));

  if (cache.Get(module_key.c_str(), &exports)) {
    return exports;
  }

  auto* mod = node::binding::get_linked_module(module_key.c_str());

  if (!mod) {
    char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "No such module: %s", module_key.c_str());
    margs->ThrowError(errmsg);
    return exports;
  }

  exports = v8::Object::New(isolate);
  DCHECK_EQ(mod->nm_register_func, nullptr);
  DCHECK_NE(mod->nm_context_register_func, nullptr);
  mod->nm_context_register_func(exports, v8::Null(isolate),
                                isolate->GetCurrentContext(), mod->nm_priv);
  cache.Set(module_key.c_str(), exports);
  return exports;
}

double Uptime() {
  // TODO(samuelmaddock): Fix imports
  // return (base::Time::Now() - base::Process::Current().CreationTime())
  //     .InSecondsF();
  return 0.0;
}

}  // namespace

// static
SandboxedWorkerObserver* SandboxedWorkerObserver::GetCurrent() {
  SandboxedWorkerObserver* self = lazy_tls.Pointer()->Get();
  return self ? self : new SandboxedWorkerObserver;
}

SandboxedWorkerObserver::SandboxedWorkerObserver() {
  lazy_tls.Pointer()->Set(this);
  metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
}

SandboxedWorkerObserver::~SandboxedWorkerObserver() {
  lazy_tls.Pointer()->Set(nullptr);
}

void SandboxedWorkerObserver::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  // Wrap the bundle into a function that receives the binding object as
  // argument.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope script_scope(isolate,
                                   v8::MicrotasksScope::kRunMicrotasks);
  v8::Local<v8::Object> binding = v8::Object::New(isolate);

  // // START INITIALIZE BINDINGS
  gin_helper::Dictionary b(isolate, binding);
  b.SetMethod("get", GetBinding);
  // b.SetMethod("createPreloadScript", CreatePreloadScript);

  gin_helper::Dictionary process = gin::Dictionary::CreateEmpty(isolate);
  b.Set("process", process);

  ElectronBindings::BindProcess(isolate, &process, metrics_.get());

  process.SetMethod("uptime", Uptime);
  process.Set("argv", base::CommandLine::ForCurrentProcess()->argv());
  process.SetReadOnly("pid", base::GetCurrentProcId());
  process.SetReadOnly("sandboxed", true);
  process.SetReadOnly("type", "worker");
  // END INITIALIZE BINDINGS

  std::vector<v8::Local<v8::String>> sandbox_preload_bundle_params = {
      node::FIXED_ONE_BYTE_STRING(isolate, "binding")
  };

  std::vector<v8::Local<v8::Value>> sandbox_preload_bundle_args = {
    binding
  };

  util::CompileAndCall(
      context, "electron/js2c/sandbox_worker_bundle",
      &sandbox_preload_bundle_params, &sandbox_preload_bundle_args, nullptr);
}

void SandboxedWorkerObserver::ContextWillDestroy(v8::Local<v8::Context> context) {
  delete this;
}

}  // namespace electron
