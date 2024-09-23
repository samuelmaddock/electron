
#include "shell/browser/api/ipc_helper.h"

#include "base/trace_event/trace_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "gin/arguments.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "shell/browser/api/electron_api_session.h"
#include "shell/browser/api/message_port.h"
#include "shell/common/gin_converters/blink_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/v8_value_serializer.h"

namespace electron {

// This object wraps the InvokeCallback so that if it gets GC'd by V8, we can
// still call the callback and send an error. Not doing so causes a Mojo DCHECK,
// since Mojo requires callbacks to be called before they are destroyed.
class ReplyChannel : public gin::Wrappable<ReplyChannel> {
 public:
  using InvokeCallback = electron::mojom::ElectronApiIPC::InvokeCallback;
  static gin::Handle<ReplyChannel> Create(v8::Isolate* isolate,
                                          InvokeCallback callback) {
    return gin::CreateHandle(isolate, new ReplyChannel(std::move(callback)));
  }

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<ReplyChannel>::GetObjectTemplateBuilder(isolate)
        .SetMethod("sendReply", &ReplyChannel::SendReply);
  }
  const char* GetTypeName() override { return "ReplyChannel"; }

  void SendError(const std::string& msg) {
    v8::Isolate* isolate = electron::JavascriptEnvironment::GetIsolate();
    // If there's no current context, it means we're shutting down, so we
    // don't need to send an event.
    if (!isolate->GetCurrentContext().IsEmpty()) {
      v8::HandleScope scope(isolate);
      auto message = gin::DataObjectBuilder(isolate).Set("error", msg).Build();
      SendReply(isolate, message);
    }
  }

 private:
  explicit ReplyChannel(InvokeCallback callback)
      : callback_(std::move(callback)) {}
  ~ReplyChannel() override {
    if (callback_)
      SendError("reply was never sent");
  }

  bool SendReply(v8::Isolate* isolate, v8::Local<v8::Value> arg) {
    if (!callback_)
      return false;
    blink::CloneableMessage message;
    if (!gin::ConvertFromV8(isolate, arg, &message)) {
      return false;
    }

    std::move(callback_).Run(std::move(message));
    return true;
  }

  InvokeCallback callback_;
};

gin::WrapperInfo ReplyChannel::kWrapperInfo = {gin::kEmbedderNativeGin};

template <typename T>
void IpcHelper<T>::Message(gin::Handle<gin_helper::internal::Event>& event,
                           const std::string& channel,
                           blink::CloneableMessage args) {
  TRACE_EVENT1("electron", "IpcHelper::Message", "channel", channel);
  EmitWithoutEvent("-ipc-message", event, channel, args);
}

template <typename T>
void IpcHelper<T>::Invoke(
    gin::Handle<gin_helper::internal::Event>& event,
    const std::string& channel,
    blink::CloneableMessage arguments,
    electron::mojom::ElectronApiIPC::InvokeCallback callback) {
  TRACE_EVENT1("electron", "IpcHelper::Invoke", "channel", channel);

  v8::Isolate* isolate = electron::JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict(isolate, event.ToV8().As<v8::Object>());
  dict.Set("_replyChannel", ReplyChannel::Create(isolate, std::move(callback)));

  EmitWithoutEvent("-ipc-invoke", event, channel, std::move(arguments));
}

template <typename T>
void IpcHelper<T>::ReceivePostMessage(
    gin::Handle<gin_helper::internal::Event>& event,
    const std::string& channel,
    blink::TransferableMessage message) {
  v8::Isolate* isolate = JavascriptEnvironment::GetIsolate();
  v8::HandleScope handle_scope(isolate);
  auto wrapped_ports =
      MessagePort::EntanglePorts(isolate, std::move(message.ports));
  v8::Local<v8::Value> message_value =
      electron::DeserializeV8Value(isolate, message);
  EmitWithoutEvent("-ipc-ports", event, channel, message_value,
                   std::move(wrapped_ports));
}

template <typename T>
void IpcHelper<T>::MessageSync(
    gin::Handle<gin_helper::internal::Event>& event,
    const std::string& channel,
    blink::CloneableMessage arguments,
    electron::mojom::ElectronApiIPC::MessageSyncCallback callback) {
  TRACE_EVENT1("electron", "IpcHelper::MessageSync", "channel", channel);

  v8::Isolate* isolate = electron::JavascriptEnvironment::GetIsolate();
  gin_helper::Dictionary dict(isolate, event.ToV8().As<v8::Object>());
  dict.Set("_replyChannel", ReplyChannel::Create(isolate, std::move(callback)));

  EmitWithoutEvent("-ipc-message-sync", event, channel, std::move(arguments));
}

// Explicit template instantiation for known classes.
template class IpcHelper<api::Session>;

}  // namespace electron
