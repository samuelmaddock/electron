// Copyright (c) 2020 Samuel Maddock <sam@samuelmaddock.com>.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ELECTRON_SHELL_BROWSER_API_ELECTRON_API_WEB_FRAME_MAIN_H_
#define ELECTRON_SHELL_BROWSER_API_ELECTRON_API_WEB_FRAME_MAIN_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "content/public/browser/global_routing_id.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "shell/browser/event_emitter_mixin.h"
#include "shell/common/gin_helper/constructible.h"
#include "shell/common/gin_helper/pinnable.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-forward.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace gin {
class Arguments;

template <typename T>
class Handle;
}  // namespace gin

namespace electron::api {

// Helper struct for serializing RenderFrameHosts to binded as a pinned
// WebFrameMain. This ensures the RenderFrameHost never gets swapped on
// cross-site navigation, in the case of race conditions.
struct PinnedRenderFrameHostRef {
 private:
  explicit PinnedRenderFrameHostRef(
      content::RenderFrameHost* render_frame_host);

 public:
  static PinnedRenderFrameHostRef Create(
      content::RenderFrameHost* render_frame_host) {
    return PinnedRenderFrameHostRef(render_frame_host);
  }

  content::GlobalRenderFrameHostToken token;
};

class WebContents;

// Bindings for accessing frames from the main process.
class WebFrameMain : public gin::Wrappable<WebFrameMain>,
                     public gin_helper::EventEmitterMixin<WebFrameMain>,
                     public gin_helper::Pinnable<WebFrameMain>,
                     public gin_helper::Constructible<WebFrameMain> {
 public:
  enum class FrameType {
    Default,  // tracks navigation across RFH swaps
    Pinned    // pins to lifespan of singular RFH
  };

  // Create a new WebFrameMain and return the V8 wrapper of it.
  static gin::Handle<WebFrameMain> New(v8::Isolate* isolate);

  static gin::Handle<WebFrameMain> From(
      v8::Isolate* isolate,
      content::RenderFrameHost* render_frame_host,
      FrameType frame_type = FrameType::Default);
  // Gets WebFrameMain if it exists, without creating a new handle
  static gin::Handle<WebFrameMain> FromOrNull(
      v8::Isolate* isolate,
      content::RenderFrameHost* render_frame_host,
      FrameType frame_type);

  static WebFrameMain* FromFrameTreeNodeId(int frame_tree_node_id);
  static WebFrameMain* FromFrameToken(
      content::GlobalRenderFrameHostToken frame_token,
      WebFrameMain::FrameType frame_type);
  static WebFrameMain* FromPinnedFrameToken(
      content::GlobalRenderFrameHostToken frame_token);
  static WebFrameMain* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host,
      FrameType frame_type = FrameType::Default);

  // gin_helper::Constructible
  static void FillObjectTemplate(v8::Isolate*, v8::Local<v8::ObjectTemplate>);
  static const char* GetClassName() { return "WebFrameMain"; }

  // gin::Wrappable
  static gin::WrapperInfo kWrapperInfo;
  const char* GetTypeName() override;

  content::RenderFrameHost* render_frame_host() const { return render_frame_; }

  // disable copy
  WebFrameMain(const WebFrameMain&) = delete;
  WebFrameMain& operator=(const WebFrameMain&) = delete;

 protected:
  explicit WebFrameMain(content::RenderFrameHost* render_frame,
                        FrameType frame_type = FrameType::Default);
  ~WebFrameMain() override;

 private:
  friend class WebContents;

  // Called when FrameTreeNode is deleted.
  void Destroyed();

  // Mark RenderFrameHost as disposed and to no longer access it. This can
  // happen when the WebFrameMain v8-forward.handle is GC'd or when a
  // FrameTreeNode is removed.
  void MarkRenderFrameDisposed();

  // Swap out the internal RFH when cross-origin navigation occurs.
  void UpdateRenderFrameHost(content::RenderFrameHost* rfh);

  const mojo::Remote<mojom::ElectronRenderer>& GetRendererApi();
  void MaybeSetupMojoConnection();
  void TeardownMojoConnection();
  void OnRendererConnectionError();

  // WebFrameMain can outlive its RenderFrameHost pointer so we need to check
  // whether its been disposed of prior to accessing it.
  bool CheckRenderFrame() const;

  v8::Local<v8::Promise> ExecuteJavaScript(gin::Arguments* args,
                                           const std::u16string& code);
  bool Reload();
  void Send(v8::Isolate* isolate,
            bool internal,
            const std::string& channel,
            v8::Local<v8::Value> args);
  void PostMessage(v8::Isolate* isolate,
                   const std::string& channel,
                   v8::Local<v8::Value> message_value,
                   std::optional<v8::Local<v8::Value>> transfer);

  bool Pinned() const;
  int FrameTreeNodeID() const;
  std::string Name() const;
  base::ProcessId OSProcessID() const;
  int ProcessID() const;
  int RoutingID() const;
  GURL URL() const;
  std::string Origin() const;
  blink::mojom::PageVisibilityState VisibilityState() const;

  content::RenderFrameHost* Top() const;
  content::RenderFrameHost* Parent() const;
  std::vector<content::RenderFrameHost*> Frames() const;
  std::vector<content::RenderFrameHost*> FramesInSubtree() const;

  std::string InternalState() const;

  void DOMContentLoaded();

  mojo::Remote<mojom::ElectronRenderer> renderer_api_;
  mojo::PendingReceiver<mojom::ElectronRenderer> pending_receiver_;

  int frame_tree_node_id_;
  content::GlobalRenderFrameHostToken frame_token_;

  raw_ptr<content::RenderFrameHost> render_frame_ = nullptr;

  // Whether the RenderFrameHost has been removed and that it should no longer
  // be accessed.
  bool render_frame_disposed_ = false;

  // Whether this instance is pinned to a RenderFrameHost and will not change
  // during its lifespan.
  bool render_frame_pinned_ = false;

  base::WeakPtrFactory<WebFrameMain> weak_factory_{this};
};

}  // namespace electron::api

#endif  // ELECTRON_SHELL_BROWSER_API_ELECTRON_API_WEB_FRAME_MAIN_H_
