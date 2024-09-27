// Copyright (c) 2024 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/renderer/preload_realm_context.h"

#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "gin/public/gin_embedders.h"
#include "shell/common/api/electron_bindings.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/event_emitter_caller.h"
#include "shell/common/node_bindings.h"
#include "shell/common/node_includes.h"
#include "shell/common/node_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"  // nogncheck
#include "third_party/blink/renderer/core/execution_context/execution_context.h"  // nogncheck
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"  // nogncheck
#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"  // nogncheck
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/script_state.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"  // nogncheck
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"  // nogncheck
#include "v8/include/v8-context.h"

namespace electron::preload_realm {

namespace {

static constexpr int kElectronContextEmbedderDataIndex =
    static_cast<int>(gin::kPerContextDataStartIndex) +
    static_cast<int>(gin::kEmbedderElectron);

const char kModuleCacheKey[] = "native-module-cache";

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

v8::Local<v8::Value> CreatePreloadScript(v8::Isolate* isolate,
                                         v8::Local<v8::String> source) {
  auto context = isolate->GetCurrentContext();
  auto maybe_script = v8::Script::Compile(context, source);
  v8::Local<v8::Script> script;
  if (!maybe_script.ToLocal(&script))
    return v8::Local<v8::Value>();
  return script->Run(context).ToLocalChecked();
}

double Uptime() {
  return (base::Time::Now() - base::Process::Current().CreationTime())
      .InSecondsF();
}

// This is a helper class to make the initiator ExecutionContext the owner
// of a ShadowRealmGlobalScope and its ScriptState. When the initiator
// ExecutionContext is destroyed, the ShadowRealmGlobalScope is destroyed,
// too.
class ShadowRealmLifetimeController
    : public blink::GarbageCollected<ShadowRealmLifetimeController>,
      public blink::ContextLifecycleObserver {
 public:
  explicit ShadowRealmLifetimeController(
      blink::ExecutionContext* initiator_execution_context,
      blink::ScriptState* initiator_script_state,
      blink::ShadowRealmGlobalScope* shadow_realm_global_scope,
      blink::ScriptState* shadow_realm_script_state,
      blink::WebServiceWorkerContextProxy* proxy)
      : initiator_script_state_(initiator_script_state),
        is_initiator_worker_or_worklet_(
            initiator_execution_context->IsWorkerOrWorkletGlobalScope()),
        shadow_realm_global_scope_(shadow_realm_global_scope),
        shadow_realm_script_state_(shadow_realm_script_state),
        proxy_(proxy) {
    // Align lifetime of this controller to that of the initiator's context.
    self_ = this;

    SetContextLifecycleNotifier(initiator_execution_context);
    RegisterDebugger(initiator_execution_context);

    realm_context()->SetAlignedPointerInEmbedderData(
        kElectronContextEmbedderDataIndex, static_cast<void*>(this));

    metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
    RunInitScript();
  }

  ~ShadowRealmLifetimeController() override {
    // DCHECK(shadow_realm_script_state_.IsSet());
    // TODO: why is this being called early?
    LOG(INFO) << "*** ~ShadowRealmLifetimeController\n";
    LOG(INFO) << base::debug::StackTrace();
  }

  static ShadowRealmLifetimeController* From(v8::Local<v8::Context> context) {
    if (context->GetNumberOfEmbedderDataFields() <=
        kElectronContextEmbedderDataIndex) {
      return nullptr;
    }
    // TODO: this is not always valid?
    auto* controller = static_cast<ShadowRealmLifetimeController*>(
        context->GetAlignedPointerFromEmbedderData(
            kElectronContextEmbedderDataIndex));
    CHECK(controller);
    return controller;
  }

  void Trace(blink::Visitor* visitor) const override {
    visitor->Trace(initiator_script_state_);
    visitor->Trace(shadow_realm_global_scope_);
    visitor->Trace(shadow_realm_script_state_);
    ContextLifecycleObserver::Trace(visitor);
  }

  v8::MaybeLocal<v8::Context> GetInitiatorContext() {
    return initiator_script_state_->ContextIsValid()
               ? initiator_script_state_->GetContext()
               : v8::MaybeLocal<v8::Context>();
  }

  blink::WebServiceWorkerContextProxy* GetServiceWorkerProxy() {
    return proxy_;
  }

 protected:
  void ContextDestroyed() override {
    LOG(INFO) << "***ShadowRealmLifetimeController::ContextDestroyed\n";
    realm_context()->SetAlignedPointerInEmbedderData(
        kElectronContextEmbedderDataIndex, nullptr);
    shadow_realm_script_state_->DisposePerContextData();
    if (is_initiator_worker_or_worklet_) {
      shadow_realm_script_state_->DissociateContext();
    }
    shadow_realm_script_state_.Clear();
    shadow_realm_global_scope_->NotifyContextDestroyed();
    shadow_realm_global_scope_.Clear();
    self_.Clear();
  }

 private:
  v8::Isolate* realm_isolate() {
    return shadow_realm_script_state_->GetIsolate();
  }
  v8::Local<v8::Context> realm_context() {
    return shadow_realm_script_state_->GetContext();
  }

  void RegisterDebugger(blink::ExecutionContext* initiator_execution_context) {
    v8::Isolate* isolate = realm_isolate();
    v8::Local<v8::Context> context = realm_context();

    if (initiator_execution_context->IsMainThreadWorkletGlobalScope()) {
      // Set the human readable name for the world.
      // DCHECK(!shadow_realm_global_scope->Name().empty());
      // world_->SetNonMainWorldHumanReadableName(world_->GetWorldId(),
      //                                          shadow_realm_global_scope->Name());
    } else {
      // Name new context for debugging. For main thread worklet global scopes
      // this is done once the context is initialized.
      blink::WorkerThreadDebugger* debugger =
          blink::WorkerThreadDebugger::From(isolate);
      const blink::KURL url_for_debugger("https://electron.org/preloadrealm");
      const auto* worker_context =
          To<blink::WorkerOrWorkletGlobalScope>(initiator_execution_context);
      debugger->ContextCreated(worker_context->GetThread(), url_for_debugger,
                               context);
    }
  }

  void RunInitScript() {
    v8::Isolate* isolate = realm_isolate();
    v8::Local<v8::Context> context = realm_context();

    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks_scope(
        isolate, context->GetMicrotaskQueue(),
        v8::MicrotasksScope::kDoNotRunMicrotasks);

    v8::Local<v8::Object> binding = v8::Object::New(isolate);

    gin_helper::Dictionary b(isolate, binding);
    b.SetMethod("get", GetBinding);
    b.SetMethod("createPreloadScript", CreatePreloadScript);

    gin_helper::Dictionary process = gin::Dictionary::CreateEmpty(isolate);
    b.Set("process", process);

    ElectronBindings::BindProcess(isolate, &process, metrics_.get());

    process.SetMethod("uptime", Uptime);
    process.Set("argv", base::CommandLine::ForCurrentProcess()->argv());
    process.SetReadOnly("pid", base::GetCurrentProcId());
    process.SetReadOnly("sandboxed", true);
    process.SetReadOnly("type", "preload_realm");
    process.SetReadOnly("contextIsolated", true);

    std::vector<v8::Local<v8::String>> preload_realm_bundle_params = {
        node::FIXED_ONE_BYTE_STRING(isolate, "binding")};

    std::vector<v8::Local<v8::Value>> preload_realm_bundle_args = {binding};

    util::CompileAndCall(context, "electron/js2c/preload_realm_bundle",
                         &preload_realm_bundle_params,
                         &preload_realm_bundle_args);
  }

  const blink::WeakMember<blink::ScriptState> initiator_script_state_;
  bool is_initiator_worker_or_worklet_;
  blink::Member<blink::ShadowRealmGlobalScope> shadow_realm_global_scope_;
  blink::Member<blink::ScriptState> shadow_realm_script_state_;
  blink::Persistent<ShadowRealmLifetimeController> self_;

  std::unique_ptr<base::ProcessMetrics> metrics_;
  raw_ptr<blink::WebServiceWorkerContextProxy> proxy_;
};

v8::Local<v8::Object> WrapShadowRealmLifetimeController(
    v8::Isolate* isolate,
    ShadowRealmLifetimeController* object) {
  v8::Local<v8::ObjectTemplate> local = v8::ObjectTemplate::New(isolate);
  local->SetInternalFieldCount(1);

  v8::Local<v8::Object> result =
      local->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
  result->SetInternalField(0, v8::External::New(isolate, object));

  return result;
}

}  // namespace

v8::MaybeLocal<v8::Context> GetInitiatorContext(
    v8::Local<v8::Context> context) {
  DCHECK(!context.IsEmpty());
  blink::ExecutionContext* execution_context =
      blink::ExecutionContext::From(context);
  if (!execution_context->IsShadowRealmGlobalScope())
    return v8::MaybeLocal<v8::Context>();
  auto* controller = ShadowRealmLifetimeController::From(context);
  if (controller)
    return controller->GetInitiatorContext();
  return v8::MaybeLocal<v8::Context>();
}

blink::WebServiceWorkerContextProxy* GetServiceWorkerProxy(
    v8::Local<v8::Context> context) {
  auto* controller = ShadowRealmLifetimeController::From(context);
  return controller ? controller->GetServiceWorkerProxy() : nullptr;
}

v8::MaybeLocal<v8::Context> OnCreatePreloadableV8Context(
    v8::Local<v8::Context> initiator_context,
    blink::WebServiceWorkerContextProxy* proxy) {
  v8::Isolate* isolate = initiator_context->GetIsolate();
  blink::ScriptState* initiator_script_state =
      blink::ScriptState::MaybeFrom(isolate, initiator_context);
  DCHECK(initiator_script_state);
  blink::ExecutionContext* initiator_execution_context =
      blink::ExecutionContext::From(initiator_context);
  DCHECK(initiator_execution_context);
  blink::DOMWrapperWorld* world = blink::DOMWrapperWorld::Create(
      isolate, blink::DOMWrapperWorld::WorldType::kShadowRealm);
  CHECK(world);  // Not yet run out of the world id.

  // Create a new ShadowRealmGlobalScope.
  blink::ShadowRealmGlobalScope* shadow_realm_global_scope =
      blink::MakeGarbageCollected<blink::ShadowRealmGlobalScope>(
          initiator_execution_context);
  const blink::WrapperTypeInfo* wrapper_type_info =
      shadow_realm_global_scope->GetWrapperTypeInfo();

  // Create a new v8::Context.
  // v8::ExtensionConfiguration* extension_configuration = nullptr;
  // Initialize V8 extensions before creating the context.
  v8::ExtensionConfiguration extension_configuration =
      blink::ScriptController::ExtensionsFor(shadow_realm_global_scope);

  v8::Local<v8::ObjectTemplate> global_template =
      wrapper_type_info->GetV8ClassTemplate(isolate, *world)
          .As<v8::FunctionTemplate>()
          ->InstanceTemplate();
  v8::Local<v8::Object> global_proxy;  // Will request a new global proxy.
  v8::Local<v8::Context> context =
      v8::Context::New(isolate, &extension_configuration, global_template,
                       global_proxy, v8::DeserializeInternalFieldsCallback(),
                       initiator_execution_context->GetMicrotaskQueue());
  context->UseDefaultSecurityToken();

  // Associate the Blink object with the v8::Context.
  blink::ScriptState* script_state =
      blink::ScriptState::Create(context, world, shadow_realm_global_scope);

  // Associate the Blink object with the v8::Objects.
  global_proxy = context->Global();
  blink::V8DOMWrapper::SetNativeInfo(isolate, global_proxy,
                                     shadow_realm_global_scope);
  v8::Local<v8::Object> global_object =
      global_proxy->GetPrototype().As<v8::Object>();
  blink::V8DOMWrapper::SetNativeInfo(isolate, global_object,
                                     shadow_realm_global_scope);

  // Install context-dependent properties.
  std::ignore =
      script_state->PerContextData()->ConstructorForType(wrapper_type_info);

  // Make the initiator execution context the owner of the
  // ShadowRealmGlobalScope and the ScriptState.
  // TODO: don't make this GC'd?
  auto* controller = blink::MakeGarbageCollected<ShadowRealmLifetimeController>(
      initiator_execution_context, initiator_script_state,
      shadow_realm_global_scope, script_state, proxy);

  v8::Local<v8::Object> wrapper =
      WrapShadowRealmLifetimeController(isolate, controller);

  gin_helper::Dictionary global(isolate, initiator_context->Global());
  global.SetHidden("preloadRealm", wrapper);

  return context;
}

}  // namespace electron::preload_realm
