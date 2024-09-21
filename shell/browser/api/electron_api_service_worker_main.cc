// Copyright (c) 2020 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_service_worker_main.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "electron/shell/common/api/api.mojom.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "shell/browser/api/message_port.h"
#include "shell/browser/browser.h"
#include "shell/browser/javascript_environment.h"
#include "shell/common/gin_converters/blink_converter.h"
#include "shell/common/gin_converters/gurl_converter.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/error_thrower.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/gin_helper/promise.h"
#include "shell/common/node_includes.h"
#include "shell/common/v8_value_serializer.h"

namespace electron::api {

// VersionId -> ServiceWorkerMain*
typedef std::unordered_map<int64_t /* version_id */, ServiceWorkerMain*>
    VersionIdMap;

VersionIdMap& GetVersionIdMap() {
  static base::NoDestructor<VersionIdMap> instance;
  return *instance;
}

// static
ServiceWorkerMain* ServiceWorkerMain::FromVersionID(int64_t version_id) {
  VersionIdMap& version_map = GetVersionIdMap();
  auto iter = version_map.find(version_id);
  auto* service_worker = iter == version_map.end() ? nullptr : iter->second;
  return service_worker;
}

gin::WrapperInfo ServiceWorkerMain::kWrapperInfo = {gin::kEmbedderNativeGin};

ServiceWorkerMain::ServiceWorkerMain(content::ServiceWorkerContext* sw_context,
                                     int64_t version_id)
    : version_id_(version_id), service_worker_context_(sw_context) {
  // Ensure SW is live
  DCHECK(service_worker_context_->IsLiveStartingServiceWorker(version_id) ||
         service_worker_context_->IsLiveRunningServiceWorker(version_id));

  GetVersionIdMap().emplace(version_id_, this);
}

ServiceWorkerMain::~ServiceWorkerMain() {
  OnDestroyed();
}

void ServiceWorkerMain::OnDestroyed() {
  version_destroyed_ = true;
  GetVersionIdMap().erase(version_id_);
  Unpin();
}

bool ServiceWorkerMain::IsDestroyed() const {
  return version_destroyed_;
}

int64_t ServiceWorkerMain::VersionID() const {
  return version_id_;
}

GURL ServiceWorkerMain::ScopeURL() const {
  // TODO:
  return GURL::EmptyGURL();
}

// static
gin::Handle<ServiceWorkerMain> ServiceWorkerMain::New(v8::Isolate* isolate) {
  return gin::Handle<ServiceWorkerMain>();
}

// static
gin::Handle<ServiceWorkerMain> ServiceWorkerMain::From(
    v8::Isolate* isolate,
    content::ServiceWorkerContext* sw_context,
    int64_t version_id) {
  auto* service_worker = FromVersionID(version_id);
  if (service_worker)
    return gin::CreateHandle(isolate, service_worker);

  auto handle =
      gin::CreateHandle(isolate, new ServiceWorkerMain(sw_context, version_id));

  // Prevent garbage collection of frame until it has been deleted internally.
  handle->Pin(isolate);

  return handle;
}

// static
void ServiceWorkerMain::FillObjectTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> templ) {
  gin_helper::ObjectTemplateBuilder(isolate, templ)
      .SetMethod("isDestroyed", &ServiceWorkerMain::IsDestroyed)
      .SetProperty("versionId", &ServiceWorkerMain::VersionID)
      .SetProperty("scope", &ServiceWorkerMain::ScopeURL)
      .Build();
}

const char* ServiceWorkerMain::GetTypeName() {
  return GetClassName();
}

}  // namespace electron::api

namespace {

using electron::api::ServiceWorkerMain;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  gin_helper::Dictionary dict(isolate, exports);
  dict.Set("ServiceWorkerMain", ServiceWorkerMain::GetConstructor(context));
}

}  // namespace

NODE_LINKED_BINDING_CONTEXT_AWARE(electron_browser_service_worker_main,
                                  Initialize)