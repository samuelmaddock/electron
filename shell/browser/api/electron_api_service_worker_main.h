// Copyright (c) 2024 Samuel Maddock <sam@samuelmaddock.com>.
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
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "shell/browser/event_emitter_mixin.h"
#include "shell/common/api/api.mojom.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/pinnable.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"

class GURL;

namespace content {
class StoragePartition;
}

namespace gin {
class Arguments;
}  // namespace gin

namespace gin_helper {
class Dictionary;
template <typename T>
class Handle;
template <typename T>
class Promise;
}  // namespace gin_helper

namespace electron::api {

// Key to uniquely identify a ServiceWorkerVersion within a StoragePartition.
struct ServiceWorkerKey {
  int64_t version_id;
  raw_ptr<const content::StoragePartition> storage_partition;

  ServiceWorkerKey(int64_t id, const content::StoragePartition* partition)
      : version_id(id), storage_partition(partition) {}

  bool operator<(const ServiceWorkerKey& other) const {
    return std::tie(version_id, storage_partition) <
           std::tie(other.version_id, other.storage_partition);
  }

  bool operator==(const ServiceWorkerKey& other) const {
    return version_id == other.version_id &&
           storage_partition == other.storage_partition;
  }

  struct Hasher {
    std::size_t operator()(const ServiceWorkerKey& key) const {
      return std::hash<const content::StoragePartition*>()(
                 key.storage_partition) ^
             std::hash<int64_t>()(key.version_id);
    }
  };
};

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
      const content::StoragePartition* storage_partition,
      int64_t version_id);
  static ServiceWorkerMain* FromVersionID(
      int64_t version_id,
      const content::StoragePartition* storage_partition);

  // gin_helper::Constructible
  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "ServiceWorkerMain"; }

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  const char* GetTypeName() override;

  // disable copy
  ServiceWorkerMain(const ServiceWorkerMain&) = delete;
  ServiceWorkerMain& operator=(const ServiceWorkerMain&) = delete;

  void OnRunningStatusChanged();
  void OnVersionRedundant();

 protected:
  explicit ServiceWorkerMain(content::ServiceWorkerContext* sw_context,
                             int64_t version_id,
                             const ServiceWorkerKey& key);
  ~ServiceWorkerMain() override;

 private:
  void Destroy();
  const blink::StorageKey GetStorageKey();

  v8::Local<v8::Promise> StartWorker(v8::Isolate* isolate);
  void DidStartWorkerForScope(gin_helper::Promise<void> promise,
                              base::Time start_time,
                              int64_t version_id,
                              int process_id,
                              int thread_id);
  void DidStartWorkerFail(gin_helper::Promise<void> promise,
                          base::Time start_time,
                          blink::ServiceWorkerStatusCode status_code);

  gin_helper::Dictionary StartExternalRequest(v8::Isolate* isolate,
                                              bool has_timeout);
  void FinishExternalRequest(v8::Isolate* isolate, std::string uuid);
  size_t CountExternalRequests();

  mojom::ElectronRenderer* GetRendererApi();

  void Send(v8::Isolate* isolate,
            bool internal,
            const std::string& channel,
            v8::Local<v8::Value> args);

  void InvalidateState();
  const content::ServiceWorkerRunningInfo* GetRunningInfo() const;

  bool IsDestroyed() const;

  int64_t VersionID() const;
  GURL ScopeURL() const;

  int64_t version_id_;
  ServiceWorkerKey key_;

  // Whether the Service Worker version has been destroyed.
  bool version_destroyed_ = false;

  // Store copy of running info when a live version isn't available
  std::unique_ptr<content::ServiceWorkerRunningInfo> running_info_;

  raw_ptr<content::ServiceWorkerContext> service_worker_context_;

  mojo::AssociatedRemote<mojom::ElectronRenderer> remote_;

  base::WeakPtrFactory<ServiceWorkerMain> weak_factory_{this};
};

}  // namespace electron::api

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_SERVICE_WORKER_MAIN_H_