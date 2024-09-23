#ifndef ELECTRON_SHELL_BROWSER_API_IPC_HELPER_H_
#define ELECTRON_SHELL_BROWSER_API_IPC_HELPER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "electron/shell/common/api/api.mojom.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "shell/browser/javascript_environment.h"
#include "shell/common/gin_helper/event.h"
#include "shell/common/gin_helper/event_emitter.h"

namespace electron {

class ElectronBrowserContext;

template <typename T>
class IpcHelper {
 public:
  IpcHelper(gin::Wrappable<T>* wrappable) : wrappable_(wrappable) {}
  ~IpcHelper() = default;

  // this.emit(name, args...);
  template <typename... Args>
  void EmitWithoutEvent(const std::string_view name, Args&&... args) {
    v8::Isolate* isolate = electron::JavascriptEnvironment::GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Object> wrapper;
    if (!wrappable_->GetWrapper(isolate).ToLocal(&wrapper))
      return;
    gin_helper::EmitEvent(isolate, wrapper, name, std::forward<Args>(args)...);
  }

  void Message(gin::Handle<gin_helper::internal::Event>& event,
               const std::string& channel,
               blink::CloneableMessage arguments);
  void Invoke(gin::Handle<gin_helper::internal::Event>& event,
              const std::string& channel,
              blink::CloneableMessage arguments,
              electron::mojom::ElectronApiIPC::InvokeCallback callback);
  void ReceivePostMessage(gin::Handle<gin_helper::internal::Event>& event,
                          const std::string& channel,
                          blink::TransferableMessage message);
  void MessageSync(
      gin::Handle<gin_helper::internal::Event>& event,
      const std::string& channel,
      blink::CloneableMessage arguments,
      electron::mojom::ElectronApiIPC::MessageSyncCallback callback);

 private:
  raw_ptr<gin::Wrappable<T>> wrappable_;
};

}  // namespace electron

#endif  // ELECTRON_SHELL_BROWSER_API_IPC_HELPER_H_
