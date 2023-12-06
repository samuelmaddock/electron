// Copyright (c) 2022 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_ELECTRON_API_SW_IPC_HANDLER_IMPL_H_
#define ELECTRON_SHELL_BROWSER_ELECTRON_API_SW_IPC_HANDLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_observer.h"
#include "electron/shell/common/api/api.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content {
class RenderProcessHost;
}

namespace electron {
class ElectronBrowserContext;

namespace api {
class Session;
}

class ElectronApiSWIPCHandlerImpl : public mojom::ElectronApiIPC,
                                    public content::RenderProcessHostObserver {
 public:
  explicit ElectronApiSWIPCHandlerImpl(
      content::RenderProcessHost* render_process_host,
      mojo::PendingAssociatedReceiver<mojom::ElectronApiIPC> receiver);

  static void BindReceiver(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::ElectronApiIPC> receiver);

  // disable copy
  ElectronApiSWIPCHandlerImpl(const ElectronApiSWIPCHandlerImpl&) = delete;
  ElectronApiSWIPCHandlerImpl& operator=(const ElectronApiSWIPCHandlerImpl&) =
      delete;
  ~ElectronApiSWIPCHandlerImpl() override;

  // mojom::ElectronApiIPC:
  void Message(bool internal,
               const std::string& channel,
               blink::CloneableMessage arguments) override;
  void Invoke(bool internal,
              const std::string& channel,
              blink::CloneableMessage arguments,
              InvokeCallback callback) override;
  void ReceivePostMessage(const std::string& channel,
                          blink::TransferableMessage message) override;
  void MessageSync(bool internal,
                   const std::string& channel,
                   blink::CloneableMessage arguments,
                   MessageSyncCallback callback) override;
  void MessageHost(const std::string& channel,
                   blink::CloneableMessage arguments) override;

  base::WeakPtr<ElectronApiSWIPCHandlerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  ElectronBrowserContext* GetBrowserContext();
  api::Session* GetSession();

  // content::RenderProcessHostObserver
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  void RemoteDisconnected();

  // Destroys this instance by removing it from the ServiceWorkerIPCList.
  void Destroy();

  // This is safe because ElectronApiSWIPCHandlerImpl is tied to the life time
  // of RenderProcessHost.
  const raw_ptr<content::RenderProcessHost> render_process_host_;

  mojo::AssociatedReceiver<mojom::ElectronApiIPC> receiver_{this};

  base::WeakPtrFactory<ElectronApiSWIPCHandlerImpl> weak_factory_{this};
};
}  // namespace electron
#endif  // ELECTRON_SHELL_BROWSER_ELECTRON_API_SW_IPC_HANDLER_IMPL_H_
