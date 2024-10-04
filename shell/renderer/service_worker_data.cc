// Copyright (c) 2024 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "electron/shell/renderer/service_worker_data.h"

#include "shell/common/heap_snapshot.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
// #include "third_party/blink/public/web/web_message_port_converter.h"

namespace electron {

ServiceWorkerData::~ServiceWorkerData() = default;

ServiceWorkerData::ServiceWorkerData(blink::WebServiceWorkerContextProxy* proxy,
                                     int64_t service_worker_version_id,
                                     const v8::Local<v8::Context>& v8_context)
    : proxy_(proxy),
      service_worker_version_id_(service_worker_version_id),
      v8_context_(v8_context->GetIsolate(), v8_context) {
  proxy_->GetAssociatedInterfaceRegistry()
      .AddInterface<mojom::ElectronRenderer>(
          base::BindRepeating(&ServiceWorkerData::OnElectronRendererRequest,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWorkerData::OnElectronRendererRequest(
    mojo::PendingAssociatedReceiver<mojom::ElectronRenderer> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

// void ServiceWorkerData::OnConnectionError() {
//   if (receiver_.is_bound())
//     receiver_.reset();
// }

void ServiceWorkerData::Message(bool internal,
                                const std::string& channel,
                                blink::CloneableMessage arguments) {
  LOG(INFO) << "***ServiceWorkerData::Message: " << channel;

  v8::Isolate* isolate = v8_context_->Isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = v8_context_;
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> args = gin::ConvertToV8(isolate, arguments);

  // TODO(samuelmaddock): emit event on preload realm context
  // EmitIPCEvent(context, internal, channel, {}, args);
}

void ServiceWorkerData::ReceivePostMessage(const std::string& channel,
                                           blink::TransferableMessage message) {
  LOG(INFO) << "***ServiceWorkerData::ReceivePostMessage: " << channel;
  // blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // if (!frame)
  //   return;

  // v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  // v8::HandleScope handle_scope(isolate);

  // v8::Local<v8::Context> context = renderer_client_->GetContext(frame,
  // isolate); v8::Context::Scope context_scope(context);

  // v8::Local<v8::Value> message_value = DeserializeV8Value(isolate, message);

  // std::vector<v8::Local<v8::Value>> ports;
  // for (auto& port : message.ports) {
  //   ports.emplace_back(
  //       blink::WebMessagePortConverter::EntangleAndInjectMessagePortChannel(
  //           context, std::move(port)));
  // }

  // std::vector<v8::Local<v8::Value>> args = {message_value};

  // EmitIPCEvent(context, false, channel, ports, gin::ConvertToV8(isolate,
  // args));
}

void ServiceWorkerData::TakeHeapSnapshot(mojo::ScopedHandle file,
                                         TakeHeapSnapshotCallback callback) {
  // Not implemented
  std::move(callback).Run(false);
}

}  // namespace electron
