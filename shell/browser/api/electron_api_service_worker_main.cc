// Copyright (c) 2024 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_service_worker_main.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"  // nogncheck
#include "content/browser/service_worker/service_worker_host.h"     // nogncheck
#include "content/browser/service_worker/service_worker_version.h"  // nogncheck
#include "content/public/browser/service_worker_running_info.h"
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
  InvalidateState();
}

ServiceWorkerMain::~ServiceWorkerMain() {
  OnDestroyed();
}

// const mojo::Remote<mojom::ElectronRenderer>&
// ServiceWorkerMain::GetRendererApi() {
//   MaybeSetupMojoConnection();
//   return renderer_api_;
// }

mojom::ElectronRenderer* ServiceWorkerMain::GetRendererApi() {
  if (!remote_.is_bound()) {
    if (!service_worker_context_->IsLiveRunningServiceWorker(version_id_)) {
      return nullptr;
    }

    service_worker_context_->GetRemoteAssociatedInterfaces(version_id_)
        .GetInterface(&remote_);
  }
  return remote_.get();
}

// void ServiceWorkerMain::MaybeSetupMojoConnection() {
//   if (version_destroyed_) {
//     // RFH may not be set yet if called between when a new RFH is created and
//     // before it's been swapped with an old RFH.
//     LOG(INFO)
//         << "Attempt to setup ServiceWorkerMain connection while render frame
//         "
//            "is disposed";
//     return;
//   }

//   if (!renderer_api_) {
//     pending_receiver_ = renderer_api_.BindNewPipeAndPassReceiver();
//     renderer_api_.set_disconnect_handler(
//         base::BindOnce(&ServiceWorkerMain::OnRendererConnectionError,
//                        weak_factory_.GetWeakPtr()));
//   }

//   // Wait for RenderFrame to be created in renderer before accessing remote.
//   if (pending_receiver_ &&
//       service_worker_context_->IsLiveRunningServiceWorker(version_id_)) {
//     // TODO: get the interface
//     service_worker_context_->GetRemoteAssociatedInterfaces(version_id_)
//         .GetInterface(&remote_);
//     // render_frame_->GetRemoteInterfaces()->GetInterface(
//     //     std::move(pending_receiver_));
//   }
// }

// void ServiceWorkerMain::TeardownMojoConnection() {
//   renderer_api_.reset();
//   pending_receiver_.reset();
// }

// void ServiceWorkerMain::OnRendererConnectionError() {
//   TeardownMojoConnection();
// }

void ServiceWorkerMain::Send(v8::Isolate* isolate,
                             bool internal,
                             const std::string& channel,
                             v8::Local<v8::Value> args) {
  blink::CloneableMessage message;
  if (!gin::ConvertFromV8(isolate, args, &message)) {
    isolate->ThrowException(v8::Exception::Error(
        gin::StringToV8(isolate, "Failed to serialize arguments")));
    return;
  }

  auto* renderer_api_remote = GetRendererApi();
  if (!renderer_api_remote) {
    return;
  }

  renderer_api_remote->Message(internal, channel, std::move(message));
}

void ServiceWorkerMain::InvalidateState() {
  if (running_info_ != nullptr) {
    running_info_.reset();
  }

  // ServiceWorkerContext saves running state
  if (service_worker_context_->IsLiveRunningServiceWorker(version_id_))
    return;

  // Need to create our own copy of running state for starting workers
  auto* wrapper = static_cast<content::ServiceWorkerContextWrapper*>(
      service_worker_context_);
  content::ServiceWorkerVersion* version = wrapper->GetLiveVersion(version_id_);
  if (version) {
    content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus
        version_status = content::ServiceWorkerRunningInfo::
            ServiceWorkerVersionStatus::kUnknown;
    content::ServiceWorkerVersionBaseInfo version_info = version->GetInfo();
    running_info_ = std::make_unique<content::ServiceWorkerRunningInfo>(
        version->script_url(), version_info.scope, version_info.storage_key,
        version_info.process_id, version->worker_host()->token(),
        version_status);
  }
}

const content::ServiceWorkerRunningInfo* ServiceWorkerMain::GetRunningInfo()
    const {
  if (service_worker_context_->IsLiveRunningServiceWorker(version_id_)) {
    auto& running_service_workers =
        service_worker_context_->GetRunningServiceWorkerInfos();
    auto it = running_service_workers.find(version_id_);
    if (it != running_service_workers.end()) {
      return &it->second;
    }
  } else if (running_info_) {
    return running_info_.get();
  }
  return nullptr;
}

void ServiceWorkerMain::OnDestroyed() {
  version_destroyed_ = true;
  running_info_.reset();
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
  auto* info = GetRunningInfo();
  if (!info)
    return GURL::EmptyGURL();
  return info->scope;
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
      .SetMethod("_send", &ServiceWorkerMain::Send)
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