#ifndef ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_
#define ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_

#include "v8/include/v8-forward.h"

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace electron {

void SetServiceWorkerProxy(v8::Local<v8::Context> context,
                           blink::WebServiceWorkerContextProxy* proxy);
blink::WebServiceWorkerContextProxy* GetServiceWorkerProxy(
    v8::Local<v8::Context> context);

v8::MaybeLocal<v8::Context> OnCreatePreloadableV8Context(
    v8::Local<v8::Context> initiator_context);

}  // namespace electron

#endif  // ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_
