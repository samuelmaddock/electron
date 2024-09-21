// Copyright (c) 2020 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SERVICE_WORKER_MAIN_H_
#define ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SERVICE_WORKER_MAIN_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "shell/browser/event_emitter_mixin.h"
#include "shell/common/api/api.mojom.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/pinnable.h"

class GURL;

namespace gin {
class Arguments;

template <typename T>
class Handle;
}  // namespace gin

namespace electron::api {

// Wraps ServiceWorkerVersion instances
class ServiceWorkerMain final
    : public gin::Wrappable<ServiceWorkerMain>,
      public gin_helper::EventEmitterMixin<ServiceWorkerMain>,
      public gin_helper::Pinnable<ServiceWorkerMain>,
      public gin_helper::Constructible<ServiceWorkerMain> {
 public:
  // Create a new ServiceWorkerMain and return the V8 wrapper of it.
  static gin::Handle<ServiceWorkerMain> New(v8::Isolate* isolate);

  static gin::Handle<ServiceWorkerMain> From(
      v8::Isolate* isolate,
      content::ServiceWorkerContext* sw_context,
      int64_t version_id);
  static ServiceWorkerMain* FromVersionID(int64_t version_id);

  // gin_helper::Constructible
  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "ServiceWorkerMain"; }

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  const char* GetTypeName() override;

  // disable copy
  ServiceWorkerMain(const ServiceWorkerMain&) = delete;
  ServiceWorkerMain& operator=(const ServiceWorkerMain&) = delete;

 protected:
  explicit ServiceWorkerMain(content::ServiceWorkerContext* sw_context,
                             int64_t version_id);
  ~ServiceWorkerMain() override;

 private:
  void InvalidateState();
  const content::ServiceWorkerRunningInfo* GetRunningInfo() const;
  void OnDestroyed();

  bool IsDestroyed() const;

  int64_t VersionID() const;
  GURL ScopeURL() const;

  int64_t version_id_;

  // Whether the Service Worker version has been destroyed.
  bool version_destroyed_ = false;

  // Store copy of running info when a live version isn't available
  std::unique_ptr<content::ServiceWorkerRunningInfo> running_info_;

  raw_ptr<content::ServiceWorkerContext> service_worker_context_;

  base::WeakPtrFactory<ServiceWorkerMain> weak_factory_{this};
};

}  // namespace electron::api

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SERVICE_WORKER_MAIN_H_