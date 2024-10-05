// Copyright (c) 2024 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_
#define ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_

#include "v8/include/v8-forward.h"

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace electron::preload_realm {

// TODO(samuelmaddock): refactor these to return preload realm controller

// Get initiator context given the preload context.
v8::MaybeLocal<v8::Context> GetInitiatorContext(v8::Local<v8::Context> context);

// Get the preload context given the initiator context.
v8::MaybeLocal<v8::Context> GetPreloadRealmContext(
    v8::Local<v8::Context> context);

// Get service worker proxy given the preload realm context.
blink::WebServiceWorkerContextProxy* GetServiceWorkerProxy(
    v8::Local<v8::Context> context);

// Create
v8::MaybeLocal<v8::Context> OnCreatePreloadableV8Context(
    v8::Local<v8::Context> initiator_context,
    blink::WebServiceWorkerContextProxy* proxy);

}  // namespace electron::preload_realm

#endif  // ELECTRON_SHELL_RENDERER_PRELOAD_REALM_CONTEXT_H_
