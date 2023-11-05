#include "shell/renderer/preload_realm_context.h"

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

namespace electron {

namespace {

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
      blink::ShadowRealmGlobalScope* shadow_realm_global_scope,
      blink::ScriptState* shadow_realm_script_state)
      : is_initiator_worker_or_worklet_(
            initiator_execution_context->IsWorkerOrWorkletGlobalScope()),
        shadow_realm_global_scope_(shadow_realm_global_scope),
        shadow_realm_script_state_(shadow_realm_script_state) {
    SetContextLifecycleNotifier(initiator_execution_context);
    RegisterDebugger(initiator_execution_context);
  }

  void Trace(blink::Visitor* visitor) const override {
    visitor->Trace(shadow_realm_global_scope_);
    visitor->Trace(shadow_realm_script_state_);
    ContextLifecycleObserver::Trace(visitor);
  }

 protected:
  void ContextDestroyed() override {
    shadow_realm_script_state_->DisposePerContextData();
    if (is_initiator_worker_or_worklet_) {
      shadow_realm_script_state_->DissociateContext();
    }
    shadow_realm_script_state_.Clear();
    shadow_realm_global_scope_->NotifyContextDestroyed();
    shadow_realm_global_scope_.Clear();
  }

 private:
  v8::Isolate* realm_isolate() {
    return shadow_realm_script_state_->GetIsolate();
  }
  v8::Local<v8::Context> realm_context() {
    return shadow_realm_script_state_->GetContext();
  }

  void RegisterDebugger(blink::ExecutionContext* initiator_execution_context) {
    v8::Local<v8::Context> context = realm_context();
    v8::Isolate* isolate = realm_isolate();

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

  bool is_initiator_worker_or_worklet_;
  blink::Member<blink::ShadowRealmGlobalScope> shadow_realm_global_scope_;
  blink::Member<blink::ScriptState> shadow_realm_script_state_;
};

}  // namespace

v8::MaybeLocal<v8::Context> OnCreatePreloadableV8Context(
    v8::Local<v8::Context> initiator_context) {
  blink::ExecutionContext* initiator_execution_context =
      blink::ExecutionContext::From(initiator_context);
  DCHECK(initiator_execution_context);
  v8::Isolate* isolate = initiator_context->GetIsolate();
  scoped_refptr<blink::DOMWrapperWorld> world = blink::DOMWrapperWorld::Create(
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
  blink::V8DOMWrapper::SetNativeInfo(isolate, global_proxy, wrapper_type_info,
                                     shadow_realm_global_scope);
  v8::Local<v8::Object> global_object =
      global_proxy->GetPrototype().As<v8::Object>();
  blink::V8DOMWrapper::SetNativeInfo(isolate, global_object, wrapper_type_info,
                                     shadow_realm_global_scope);

  // Install context-dependent properties.
  std::ignore =
      script_state->PerContextData()->ConstructorForType(wrapper_type_info);

  // Make the initiator execution context the owner of the
  // ShadowRealmGlobalScope and the ScriptState.
  blink::MakeGarbageCollected<ShadowRealmLifetimeController>(
      initiator_execution_context, shadow_realm_global_scope, script_state);

  LOG(INFO) << "***Created PreloadRealm context\n";

  // TODO: why is URL not set in shadow realm even though its set to
  // `Exposed=*` in WebIDL?

  return context;
}

}  // namespace electron
